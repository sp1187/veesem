#include "spu.h"

#include "bus_interface.h"
#include "cpu.h"
#include "irq.h"

#include <algorithm>

static const int kEnvelopeFrameDivides[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13};

static const int kRampdownFrameDivides[] = {2, 4, 6, 8, 10, 12, 13, 13};

static const int kPitchbendFrameDivides[] = {3, 4, 5, 6, 7, 8, 9, 10};

Spu::Spu(BusInterface& bus, Irq& irq) : bus_(bus), irq_(irq) {}

void Spu::Reset() {
  audio_buffer_pos_ = 0;
  sample_clock_.Reset();
  envelope_clock_.Reset();
  rampdown_clock_.Reset();

  channel_data_.fill({});
  channel_enable_.reset();
  channel_fiq_enable_.reset();
  channel_fiq_status_.reset();
  channel_env_rampdown_.reset();
  channel_stop_.reset();
  channel_zero_cross_.reset();
  channel_repeat_.reset();
  channel_env_mode_.reset();
  channel_tone_release_.reset();
  channel_env_irq_.reset();
  channel_pitch_bend_.reset();

  main_volume_ = 0;
  wave_out_l_ = 0x8000;
  wave_out_r_ = 0x8000;
  beat_base_count_ = 0;
  current_beat_base_count_ = 0;
  beat_count_.raw = 0;
  control_.raw = 0;
}

void Spu::RunCycles(int cycles) {
  if (sample_clock_.Tick(cycles)) {
    GenerateSample();
  }

  if (envelope_clock_.Tick(cycles)) {
    UpdateEnvelopes();

    if (rampdown_clock_.Tick(1)) {
      UpdateRampdowns();
    }

    if (current_beat_base_count_) {
      current_beat_base_count_--;

      if (current_beat_base_count_ == 0) {
        current_beat_base_count_ = beat_base_count_;
        if (beat_count_.beat_count) {
          beat_count_.beat_count--;
        }

        if (beat_count_.beat_count == 0 && beat_count_.irq_enable) {
          beat_count_.irq_status = true;
          UpdateBeatIrq();
        }
      }
    }
  }
}

void Spu::GenerateSample() {
  int32_t left_out = 0;
  int32_t right_out = 0;
  for (int channel_index = 0; channel_index < 16; channel_index++) {
    const auto& channel = channel_data_[channel_index];
    if (!channel_enable_[channel_index] || channel_stop_[channel_index])
      continue;
    TickChannel(channel_index);

    uint16_t prev_sample_part =
        (static_cast<uint64_t>(channel.wave_data_0) * ((1 << 19) - channel.phase_acc)) >> 19;
    uint16_t cur_sample_part = (static_cast<uint64_t>(channel.wave_data) * channel.phase_acc) >> 19;
    int16_t sample = (prev_sample_part + cur_sample_part) - 0x8000;

    const auto& pan = channel.pan;
    int left_pan = std::clamp((0x80 - static_cast<int>(pan.pan)) * 2, 0x0, 0x7f);
    int right_pan = std::clamp(static_cast<int>(pan.pan) * 2, 0x0, 0x7f);

    sample = (sample * static_cast<int>(channel.envelope_data.edd)) >> 7;

    left_out += (sample * left_pan * static_cast<int>(pan.volume)) >> 14;
    right_out += (sample * right_pan * static_cast<int>(pan.volume)) >> 14;
  }

  int16_t left_final = (left_out >> (4 - control_.high_volume)) * main_volume_ >> 7;
  int16_t right_final = (right_out >> (4 - control_.high_volume)) * main_volume_ >> 7;

  wave_out_l_ = left_final ^ 0x8000;
  wave_out_r_ = right_final ^ 0x8000;

  audio_buffer_[audio_buffer_pos_++] = wave_out_l_;
  audio_buffer_[audio_buffer_pos_++] = wave_out_r_;
  if (audio_buffer_pos_ == audio_buffer_.size() + 1)
    audio_buffer_pos_ = 0;
}

void Spu::UpdateEnvelopes() {
  for (int channel_index = 0; channel_index < 16; channel_index++) {
    if (!channel_enable_[channel_index] || channel_stop_[channel_index])
      continue;
    TickChannelEnvelope(channel_index);
    TickChannelPitchbend(channel_index);
  }
}

void Spu::UpdateRampdowns() {
  for (int channel_index = 0; channel_index < 16; channel_index++) {
    if (!channel_enable_[channel_index] || channel_stop_[channel_index])
      continue;
    TickChannelRampdown(channel_index);
  }
}

void Spu::TickChannel(int channel_index) {
  auto& channel = channel_data_[channel_index];
  uint32_t phase_acc = channel.phase_acc + channel.phase;
  channel.phase_acc = phase_acc & 0x7ffff;
  if (phase_acc >= 0x80000) {
    if (channel_fiq_enable_[channel_index]) {
      channel_fiq_status_[channel_index] = true;
      UpdateChannelIrq();
    }

    channel.wave_data_0 = channel.wave_data;

    if (channel.mode.tone_mode == 0)
      return;  // TODO

    word_t word = bus_.ReadWord(channel.wave_address);

    if (channel.mode.adpcm) {
      if (word == 0xffff) {
        HandleEndMarker(channel_index);
      } else {
        const uint8_t adpcm_value = (word >> channel.wave_shift) & 0xf;
        channel.wave_data = channel.adpcm.Decode(adpcm_value) ^ 0x8000;
      }

      channel.wave_shift += 4;
      if (channel.wave_shift >= 16) {
        channel.wave_shift = 0;
        channel.wave_address++;
      }
    } else if (!channel.mode.tone_color) {
      const uint8_t pcm_value = (word >> channel.wave_shift) & 0xff;
      if (pcm_value == 0xff) {
        HandleEndMarker(channel_index);
      } else {
        channel.wave_data = (pcm_value << 8) | pcm_value;
      }

      channel.wave_shift += 8;
      if (channel.wave_shift >= 16) {
        channel.wave_shift = 0;
        channel.wave_address++;
      }
    } else {
      if (word == 0xffff) {
        HandleEndMarker(channel_index);
      } else {
        channel.wave_data = word;
      }

      channel.wave_address++;
    }
  }
}

void Spu::HandleEndMarker(int channel_index) {
  auto& channel = channel_data_[channel_index];
  if (channel.mode.tone_mode == 1) {
    StopChannel(channel_index);
  } else if (channel.mode.tone_mode == 2) {
    channel.wave_address = channel.loop_address;
    channel.wave_shift = 0;
    channel.mode.adpcm = false;
  }
}

void Spu::TickChannelEnvelope(int channel_index) {
  auto& channel = channel_data_[channel_index];
  if (!channel_env_mode_[channel_index] && !channel_env_rampdown_[channel_index] &&
      envelope_clock_.GetDividedTick(kEnvelopeFrameDivides[channel.env_clk])) {
    if (channel.envelope_data.count) {
      channel.envelope_data.count--;
    }

    if (channel.envelope_data.count == 0) {
      if (channel.envelope_data.edd != channel.envelope0.target) {
        if (channel.envelope0.sign) {
          channel.envelope_data.edd = std::clamp(
              static_cast<int>(channel.envelope_data.edd) - static_cast<int>(channel.envelope0.inc),
              static_cast<int>(channel.envelope0.target), 0x7f);

          if (channel.envelope_data.edd == 0) {
            StopChannel(channel_index);
            return;
          }
        } else {
          channel.envelope_data.edd = std::clamp(
              static_cast<int>(channel.envelope_data.edd) + static_cast<int>(channel.envelope0.inc),
              0, static_cast<int>(channel.envelope0.target));
        }
      }

      if (channel.envelope_data.edd == channel.envelope0.target) {
        addr_t addr = channel.envelope_address + channel.envelope_loop_control.ea_offset;
        if (channel.envelope1.repeat) {
          if (channel.envelope1.repeat_count) {
            channel.envelope1.repeat_count--;
          }

          if (channel.envelope1.repeat_count == 0) {
            channel.envelope0.raw = bus_.ReadWord(addr);
            channel.envelope1.raw = bus_.ReadWord(addr + 1);
            auto old_rampdown_offset =
                static_cast<int>(channel.envelope_loop_control.rampdown_offset);
            channel.envelope_loop_control.raw = bus_.ReadWord(addr + 2);
            channel.envelope_loop_control.rampdown_offset = old_rampdown_offset;

            if (channel.envelope_irq.irq_enable &&
                channel.envelope_loop_control.ea_offset == channel.envelope_irq.irq_fire_address) {
              channel_env_irq_[channel_index] = true;
              UpdateBeatIrq();
            }
          }
        } else {
          channel.envelope0.raw = bus_.ReadWord(addr);
          channel.envelope1.raw = bus_.ReadWord(addr + 1);
          channel.envelope_loop_control.ea_offset += 2;

          if (channel.envelope_irq.irq_enable &&
              channel.envelope_loop_control.ea_offset == channel.envelope_irq.irq_fire_address) {
            channel_env_irq_[channel_index] = true;
            UpdateBeatIrq();
          }
        }
      }

      channel.envelope_data.count = static_cast<int>(channel.envelope1.load);
    }
  }
}

void Spu::TickChannelPitchbend(int channel_index) {
  auto& channel = channel_data_[channel_index];
  if (channel_pitch_bend_[channel_index] && channel.phase != channel.target_phase &&
      envelope_clock_.GetDividedTick(
          kPitchbendFrameDivides[channel.pitch_bend_control.time_step])) {
    if (channel.pitch_bend_control.sign) {
      channel.phase = std::clamp(
          static_cast<int>(channel.phase) - static_cast<int>(channel.pitch_bend_control.offset),
          static_cast<int>(channel.target_phase), 0x7ffff);
    } else {
      channel.phase = std::clamp(
          static_cast<int>(channel.phase) + static_cast<int>(channel.pitch_bend_control.offset), 0,
          static_cast<int>(channel.target_phase));
    }
  }
}

void Spu::TickChannelRampdown(int channel_index) {
  auto& channel = channel_data_[channel_index];
  if (channel_env_rampdown_[channel_index] &&
      rampdown_clock_.GetDividedTick(kRampdownFrameDivides[channel.rampdown_clk])) {
    channel.envelope_data.edd =
        std::clamp(static_cast<int>(channel.envelope_data.edd) -
                       static_cast<int>(channel.envelope_loop_control.rampdown_offset),
                   0, 0x7f);

    if (channel.envelope_data.edd == 0) {
      StopChannel(channel_index);
      channel_env_rampdown_[channel_index] = false;
      channel_tone_release_[channel_index] = false;
    }
  }
}

void Spu::StartChannel(int channel_index) {
  auto& channel = channel_data_[channel_index];

  channel.wave_shift = 0;
  channel.adpcm.Reset();
  if (!channel_env_mode_[channel_index]) {
    channel.envelope_data.count = static_cast<int>(channel.envelope1.load);
  }
}

void Spu::StopChannel(int channel_index) {
  channel_stop_[channel_index] = true;

  channel_tone_release_[channel_index] = false;
  channel_env_rampdown_[channel_index] = false;
  channel_data_[channel_index].mode.adpcm = false;
}

void Spu::UpdateChannelIrq() {
  bool channel_fiq_enabled = channel_fiq_status_.any();
  irq_.SetSpuChannelIrq(channel_fiq_enabled);
}

void Spu::UpdateBeatIrq() {
  bool beat_enabled = beat_count_.irq_enable && beat_count_.irq_status;
  bool envirq_enabled = channel_env_irq_.any();
  irq_.SetSpuBeatIrq(beat_enabled || envirq_enabled);
}

std::span<uint16_t> Spu::GetAudio() {
  auto size = audio_buffer_pos_;
  audio_buffer_pos_ = 0;
  return {audio_buffer_.data(), size};
}

word_t Spu::GetWaveAddressLo(int channel_index) {
  return channel_data_[channel_index].wave_address & 0xffff;
}

void Spu::SetWaveAddressLo(int channel_index, word_t value) {
  auto& wave_address = channel_data_[channel_index].wave_address;
  wave_address = (wave_address & ~0xffff) | value;
  channel_data_[channel_index].wave_shift = 0;
}

word_t Spu::GetMode(int channel_index) {
  const uint16_t wave_address_hi = channel_data_[channel_index].wave_address >> 16;
  const uint16_t loop_address_hi = channel_data_[channel_index].loop_address >> 16;
  return channel_data_[channel_index].mode.raw | (loop_address_hi << 6) | wave_address_hi;
}

void Spu::SetMode(int channel_index, word_t value) {
  channel_data_[channel_index].mode.raw = value & ChannelData::Mode::WriteMask;
  auto& wave_address = channel_data_[channel_index].wave_address;
  auto& loop_address = channel_data_[channel_index].loop_address;
  wave_address = ((value & 0x3f) << 16) | (wave_address & 0xffff);
  loop_address = (((value >> 6) & 0x3f) << 16) | (loop_address & 0xffff);
}

word_t Spu::GetLoopAddressLo(int channel_index) {
  return channel_data_[channel_index].loop_address & 0xffff;
}

void Spu::SetLoopAddressLo(int channel_index, word_t value) {
  auto& loop_address = channel_data_[channel_index].loop_address;
  loop_address = (loop_address & ~0xffff) | value;
}

word_t Spu::GetPan(int channel_index) {
  return channel_data_[channel_index].pan.raw;
}

void Spu::SetPan(int channel_index, word_t value) {
  channel_data_[channel_index].pan.raw = value & ChannelData::Pan::WriteMask;
}

word_t Spu::GetEnvelope0(int channel_index) {
  return channel_data_[channel_index].envelope0.raw;
}

void Spu::SetEnvelope0(int channel_index, word_t value) {
  channel_data_[channel_index].envelope0.raw = value & ChannelData::Envelope0::WriteMask;
}

word_t Spu::GetEnvelopeData(int channel_index) {
  return channel_data_[channel_index].envelope_data.raw;
}

void Spu::SetEnvelopeData(int channel_index, word_t value) {
  channel_data_[channel_index].envelope_data.raw = value & ChannelData::EnvelopeData::WriteMask;
}

word_t Spu::GetEnvelope1(int channel_index) {
  return channel_data_[channel_index].envelope1.raw;
}

void Spu::SetEnvelope1(int channel_index, word_t value) {
  channel_data_[channel_index].envelope1.raw = value;
}

word_t Spu::GetEnvelopeAddressHi(int channel_index) {
  const uint16_t envelope_address_hi = channel_data_[channel_index].envelope_address >> 16;
  return channel_data_[channel_index].envelope_irq.raw | envelope_address_hi;
}

void Spu::SetEnvelopeAddressHi(int channel_index, word_t value) {
  channel_data_[channel_index].envelope_irq.raw = value & ChannelData::EnvelopeIrq::WriteMask;
  auto& envelope_address = channel_data_[channel_index].envelope_address;
  envelope_address = ((value & 0x3f) << 16) | (envelope_address & 0xffff);
}

word_t Spu::GetEnvelopeAddressLo(int channel_index) {
  return channel_data_[channel_index].envelope_address & 0xffff;
}

void Spu::SetEnvelopeAddressLo(int channel_index, word_t value) {
  auto& envelope_address = channel_data_[channel_index].envelope_address;
  envelope_address = (envelope_address & ~0xffff) | value;
}

word_t Spu::GetWaveData0(int channel_index) {
  return channel_data_[channel_index].wave_data_0;
}

void Spu::SetWaveData0(int channel_index, word_t value) {
  channel_data_[channel_index].wave_data_0 = value;
}

word_t Spu::GetEnvelopeLoopControl(int channel_index) {
  return channel_data_[channel_index].envelope_loop_control.raw;
}

void Spu::SetEnvelopeLoopControl(int channel_index, word_t value) {
  channel_data_[channel_index].envelope_loop_control.raw = value;
}

word_t Spu::GetWaveData(int channel_index) {
  return channel_data_[channel_index].wave_data;
}

void Spu::SetWaveData(int channel_index, word_t value) {
  channel_data_[channel_index].wave_data = value;
}

word_t Spu::GetPhaseHi(int channel_index) {
  return channel_data_[channel_index].phase >> 16;
}

void Spu::SetPhaseHi(int channel_index, word_t value) {
  auto& phase = channel_data_[channel_index].phase;
  phase = ((value & 0x07) << 16) | (phase & 0xffff);
}

word_t Spu::GetPhaseAccumulatorHi(int channel_index) {
  return channel_data_[channel_index].phase_acc >> 16;
}

void Spu::SetPhaseAccumulatorHi(int channel_index, word_t value) {
  auto& phase_acc = channel_data_[channel_index].phase_acc;
  phase_acc = ((value & 0x07) << 16) | (phase_acc & 0xffff);
}

word_t Spu::GetTargetPhaseHi(int channel_index) {
  return channel_data_[channel_index].target_phase >> 16;
}

void Spu::SetTargetPhaseHi(int channel_index, word_t value) {
  auto& target_phase = channel_data_[channel_index].target_phase;
  target_phase = ((value & 0x07) << 16) | (target_phase & 0xffff);
}

word_t Spu::GetRampDownClock(int channel_index) {
  return channel_data_[channel_index].rampdown_clk;
}

void Spu::SetRampDownClock(int channel_index, word_t value) {
  channel_data_[channel_index].rampdown_clk = value & 0x07;
}

word_t Spu::GetPhaseLo(int channel_index) {
  return channel_data_[channel_index].phase & 0xffff;
}

void Spu::SetPhaseLo(int channel_index, word_t value) {
  auto& phase = channel_data_[channel_index].phase;
  phase = (phase & ~0xffff) | value;
}

word_t Spu::GetPhaseAccumulatorLo(int channel_index) {
  return channel_data_[channel_index].phase_acc & 0xffff;
}

void Spu::SetPhaseAccumulatorLo(int channel_index, word_t value) {
  auto& phase_acc = channel_data_[channel_index].phase_acc;
  phase_acc = (phase_acc & ~0xffff) | value;
}

word_t Spu::GetTargetPhaseLo(int channel_index) {
  return channel_data_[channel_index].target_phase & 0xffff;
}

void Spu::SetTargetPhaseLo(int channel_index, word_t value) {
  auto& target_phase = channel_data_[channel_index].target_phase;
  target_phase = (target_phase & ~0xffff) | value;
}

word_t Spu::GetPitchBendControl(int channel_index) {
  return channel_data_[channel_index].pitch_bend_control.raw;
}

void Spu::SetPitchBendControl(int channel_index, word_t value) {
  channel_data_[channel_index].pitch_bend_control.raw = value;
}

word_t Spu::GetChannelEnable() {
  return channel_enable_.to_ulong();
}

void Spu::SetChannelEnable(word_t value) {
  auto old_channel_enable = channel_enable_;
  channel_enable_ = value;
  for (int channel_index = 0; channel_index < 16; channel_index++) {
    if (old_channel_enable[channel_index] == channel_enable_[channel_index])
      continue;

    if (channel_stop_[channel_index])
      continue;

    if (channel_enable_[channel_index]) {
      StartChannel(channel_index);
    } else {
      StopChannel(channel_index);
    }
  }
}

word_t Spu::GetMainVolume() {
  return main_volume_;
}

void Spu::SetMainVolume(word_t value) {
  main_volume_ = value & 0x7f;
}

word_t Spu::GetChannelFiqEnable() {
  return channel_fiq_enable_.to_ulong();
}

void Spu::SetChannelFiqEnable(word_t value) {
  channel_fiq_enable_ = value;
}

word_t Spu::GetChannelFiqStatus() {
  return channel_fiq_status_.to_ulong();
}

void Spu::ClearChannelFiqStatus(word_t value) {
  channel_fiq_status_ &= ~value;
  UpdateChannelIrq();
}

word_t Spu::GetBeatBaseCount() {
  return beat_base_count_;
}

void Spu::SetBeatBaseCount(word_t value) {
  beat_base_count_ = value & 0xfff;
  current_beat_base_count_ = beat_base_count_;
}

word_t Spu::GetBeatCount() {
  return beat_count_.raw;
}

void Spu::SetBeatCount(word_t value) {
  bool old_irq_status = beat_count_.irq_status;
  beat_count_.raw = value;
  beat_count_.irq_status = old_irq_status && !beat_count_.irq_status;
  UpdateBeatIrq();
}

word_t Spu::GetEnvClk0_3() {
  return channel_data_[0].env_clk | (channel_data_[1].env_clk << 4) |
         (channel_data_[2].env_clk << 8) | (channel_data_[3].env_clk << 12);
}

void Spu::SetEnvClk0_3(word_t value) {
  channel_data_[0].env_clk = value & 0x0f;
  channel_data_[1].env_clk = (value >> 4) & 0x0f;
  channel_data_[2].env_clk = (value >> 8) & 0x0f;
  channel_data_[3].env_clk = (value >> 12) & 0x0f;
}

word_t Spu::GetEnvClk4_7() {
  return channel_data_[4].env_clk | (channel_data_[5].env_clk << 4) |
         (channel_data_[6].env_clk << 8) | (channel_data_[7].env_clk << 12);
}

void Spu::SetEnvClk4_7(word_t value) {
  channel_data_[4].env_clk = value & 0x0f;
  channel_data_[5].env_clk = (value >> 4) & 0x0f;
  channel_data_[6].env_clk = (value >> 8) & 0x0f;
  channel_data_[7].env_clk = (value >> 12) & 0x0f;
}

word_t Spu::GetEnvClk8_11() {
  return channel_data_[8].env_clk | (channel_data_[9].env_clk << 4) |
         (channel_data_[10].env_clk << 8) | (channel_data_[11].env_clk << 12);
}

void Spu::SetEnvClk8_11(word_t value) {
  channel_data_[8].env_clk = value & 0x0f;
  channel_data_[9].env_clk = (value >> 4) & 0x0f;
  channel_data_[10].env_clk = (value >> 8) & 0x0f;
  channel_data_[11].env_clk = (value >> 12) & 0x0f;
}

word_t Spu::GetEnvClk12_15() {
  return channel_data_[12].env_clk | (channel_data_[13].env_clk << 4) |
         (channel_data_[14].env_clk << 8) | (channel_data_[15].env_clk << 12);
}

void Spu::SetEnvClk12_15(word_t value) {
  channel_data_[12].env_clk = value & 0x0f;
  channel_data_[13].env_clk = (value >> 4) & 0x0f;
  channel_data_[14].env_clk = (value >> 8) & 0x0f;
  channel_data_[15].env_clk = (value >> 12) & 0x0f;
}

word_t Spu::GetEnvRampdown() {
  return channel_env_rampdown_.to_ulong();
}

void Spu::SetEnvRampdown(word_t value) {
  channel_env_rampdown_ = value;
}

word_t Spu::GetChannelStop() {
  return channel_stop_.to_ulong();
}

void Spu::ClearChannelStop(word_t value) {
  auto old_channel_stop = channel_stop_;
  channel_stop_ &= ~value;

  for (int channel_index = 0; channel_index < 16; channel_index++) {
    if (old_channel_stop[channel_index] == channel_stop_[channel_index])
      continue;

    if (channel_enable_[channel_index] && !channel_stop_[channel_index]) {
      StartChannel(channel_index);
    }
  }
}

word_t Spu::GetChannelZeroCross() {
  return channel_zero_cross_.to_ulong();
}

void Spu::SetChannelZeroCross(word_t value) {
  channel_zero_cross_ = value;
}

word_t Spu::GetControl() {
  return control_.raw;
}

void Spu::SetControl(word_t value) {
  bool old_overflow = control_.overflow;
  control_.raw = value & Control::WriteMask;
  control_.overflow = old_overflow;
}

word_t Spu::GetChannelStatus() {
  return channel_enable_.to_ulong() & ~channel_stop_.to_ulong();
}

void Spu::SetWaveInLeft(word_t value) {}
void Spu::SetWaveInRight(word_t value) {}

word_t Spu::GetWaveOutLeft() {
  return wave_out_l_;
}

word_t Spu::GetWaveOutRight() {
  return wave_out_r_;
}

word_t Spu::GetChannelRepeat() {
  return channel_repeat_.to_ulong();
}

void Spu::SetChannelRepeat(word_t value) {
  channel_repeat_ = value;
}

word_t Spu::GetChannelEnvMode() {
  return channel_env_mode_.to_ulong();
}

void Spu::SetChannelEnvMode(word_t value) {
  channel_env_mode_ = value;
}

word_t Spu::GetChannelToneRelease() {
  return channel_tone_release_.to_ulong();
}

void Spu::SetChannelToneRelease(word_t value) {
  channel_tone_release_ = value;
}

word_t Spu::GetChannelEnvIrq() {
  return channel_env_irq_.to_ulong();
}

void Spu::ClearChannelEnvIrq(word_t value) {
  channel_env_irq_ &= ~value;
}

word_t Spu::GetChannelPitchBend() {
  return channel_pitch_bend_.to_ulong();
}

void Spu::SetChannelPitchBend(word_t value) {
  channel_pitch_bend_ = value;
}
