#pragma once

#include "adpcm.h"
#include "core/common.h"

#include <array>
#include <bitset>
#include <fstream>

class BusInterface;
class Irq;

class Spu {
public:
  Spu(BusInterface& bus, Irq& irq_);

  void Reset();
  void RunCycles(int cycles);

  std::span<uint16_t> GetAudio();

  /* 30xx values */
  word_t GetWaveAddressLo(int channel_index);
  void SetWaveAddressLo(int channel_index, word_t value);
  word_t GetMode(int channel_index);
  void SetMode(int channel_index, word_t value);
  word_t GetLoopAddressLo(int channel_index);
  void SetLoopAddressLo(int channel_index, word_t value);
  word_t GetPan(int channel_index);
  void SetPan(int channel_index, word_t value);
  word_t GetEnvelope0(int channel_index);
  void SetEnvelope0(int channel_index, word_t value);
  word_t GetEnvelopeData(int channel_index);
  void SetEnvelopeData(int channel_index, word_t value);
  word_t GetEnvelope1(int channel_index);
  void SetEnvelope1(int channel_index, word_t value);
  word_t GetEnvelopeAddressHi(int channel_index);
  void SetEnvelopeAddressHi(int channel_index, word_t value);
  word_t GetEnvelopeAddressLo(int channel_index);
  void SetEnvelopeAddressLo(int channel_index, word_t value);
  word_t GetWaveData0(int channel_index);
  void SetWaveData0(int channel_index, word_t value);
  word_t GetEnvelopeLoopControl(int channel_index);
  void SetEnvelopeLoopControl(int channel_index, word_t value);
  word_t GetWaveData(int channel_index);
  void SetWaveData(int channel_index, word_t value);

  /* 32xx values */
  word_t GetPhaseHi(int channel_index);
  void SetPhaseHi(int channel_index, word_t value);
  word_t GetPhaseAccumulatorHi(int channel_index);
  void SetPhaseAccumulatorHi(int channel_index, word_t value);
  word_t GetTargetPhaseHi(int channel_index);
  void SetTargetPhaseHi(int channel_index, word_t value);
  word_t GetRampDownClock(int channel_index);
  void SetRampDownClock(int channel_index, word_t value);
  word_t GetPhaseLo(int channel_index);
  void SetPhaseLo(int channel_index, word_t value);
  word_t GetPhaseAccumulatorLo(int channel_index);
  void SetPhaseAccumulatorLo(int channel_index, word_t value);
  word_t GetTargetPhaseLo(int channel_index);
  void SetTargetPhaseLo(int channel_index, word_t value);
  word_t GetPitchBendControl(int channel_index);
  void SetPitchBendControl(int channel_index, word_t value);

  /* 34xx values */
  word_t GetChannelEnable();
  void SetChannelEnable(word_t value);
  word_t GetMainVolume();
  void SetMainVolume(word_t value);
  word_t GetChannelFiqEnable();
  void SetChannelFiqEnable(word_t value);
  word_t GetChannelFiqStatus();
  void ClearChannelFiqStatus(word_t value);
  word_t GetBeatBaseCount();
  void SetBeatBaseCount(word_t value);
  word_t GetBeatCount();
  void SetBeatCount(word_t value);
  word_t GetEnvClk0_3();
  void SetEnvClk0_3(word_t value);
  word_t GetEnvClk4_7();
  void SetEnvClk4_7(word_t value);
  word_t GetEnvClk8_11();
  void SetEnvClk8_11(word_t value);
  word_t GetEnvClk12_15();
  void SetEnvClk12_15(word_t value);
  word_t GetEnvRampdown();
  void SetEnvRampdown(word_t value);
  word_t GetChannelStop();
  void ClearChannelStop(word_t value);
  word_t GetChannelZeroCross();
  void SetChannelZeroCross(word_t value);
  word_t GetControl();
  void SetControl(word_t value);
  word_t GetCompressControl();
  void SetCompressControl(word_t value);
  word_t GetChannelStatus();
  void SetWaveInLeft(word_t value);
  void SetWaveInRight(word_t value);
  word_t GetWaveOutLeft();
  void SetWaveOutLeft(word_t value);
  word_t GetWaveOutRight();
  void SetWaveOutRight(word_t value);
  word_t GetChannelRepeat();
  void SetChannelRepeat(word_t value);
  word_t GetChannelEnvMode();
  void SetChannelEnvMode(word_t value);
  word_t GetChannelToneRelease();
  void SetChannelToneRelease(word_t value);
  word_t GetChannelEnvIrq();
  void ClearChannelEnvIrq(word_t value);
  word_t GetChannelPitchBend();
  void SetChannelPitchBend(word_t value);

private:
  void GenerateSample();
  void UpdateEnvelopes();
  void UpdateRampdowns();
  void TickChannel(int channel_index);
  void HandleEndMarker(int channel_index);
  void TickChannelEnvelope(int channel_index);
  void TickChannelPitchbend(int channel_index);
  void TickChannelRampdown(int channel_index);
  void StartChannel(int channel_index);
  void StopChannel(int channel_index);

  void UpdateChannelIrq();
  void UpdateBeatIrq();

  std::array<uint16_t, 6144 * 2> audio_buffer_;
  size_t audio_buffer_pos_;
  SimpleClock<96> sample_clock_;
  DivisibleClock<384> envelope_clock_;
  DivisibleClock<13> rampdown_clock_;

  struct ChannelData {
    addr_t wave_address = 0;
    addr_t loop_address = 0;
    uint8_t wave_shift = 0;
    addr_t envelope_address = 0;
    union Mode {
      word_t raw = 0;
      Bitfield<15, 1> adpcm;
      Bitfield<14, 1> tone_color;
      Bitfield<12, 2> tone_mode;

      static const word_t WriteMask = 0xf000;
    } mode;
    union Pan {
      word_t raw = 0;
      Bitfield<8, 7> pan;
      Bitfield<0, 7> volume;

      static const word_t WriteMask = 0x7f7f;
    } pan;
    union Envelope0 {
      word_t raw = 0;
      Bitfield<8, 7> target;
      Bitfield<7, 1> sign;
      Bitfield<0, 7> inc;
      static const word_t WriteMask = 0x7fff;
    } envelope0;
    union Envelope1 {
      word_t raw = 0;
      Bitfield<9, 7> repeat_count;
      Bitfield<8, 1> repeat;
      Bitfield<0, 8> load;
    } envelope1;
    union EnvelopeIrq {
      word_t raw = 0;
      Bitfield<7, 9> irq_fire_address;
      Bitfield<6, 1> irq_enable;

      static const word_t WriteMask = 0xffc0;
    } envelope_irq;
    union EnvelopeData {
      word_t raw = 0;
      Bitfield<8, 8> count;
      Bitfield<0, 7> edd;

      static const word_t WriteMask = 0xff7f;
    } envelope_data;
    union EnvelopeLoopControl {
      word_t raw = 0;
      Bitfield<9, 7> rampdown_offset;
      Bitfield<0, 9> ea_offset;
    } envelope_loop_control;
    uint16_t wave_data_0 = 0x8000;
    uint16_t wave_data = 0x8000;
    uint32_t phase = 0;
    uint32_t phase_acc = 0;
    uint32_t target_phase = 0;
    uint8_t env_clk = 0;
    uint8_t rampdown_clk = 0;
    union PitchBendControl {
      word_t raw = 0;
      Bitfield<13, 3> time_step;
      Bitfield<12, 1> sign;
      Bitfield<0, 12> offset;
    } pitch_bend_control;
    Adpcm adpcm;
  };

  std::array<ChannelData, 16> channel_data_;

  std::bitset<16> channel_enable_;
  std::bitset<16> channel_fiq_enable_;
  std::bitset<16> channel_fiq_status_;
  std::bitset<16> channel_env_rampdown_;
  std::bitset<16> channel_stop_;
  std::bitset<16> channel_zero_cross_;
  std::bitset<16> channel_status_;
  std::bitset<16> channel_repeat_;
  std::bitset<16> channel_env_mode_;
  std::bitset<16> channel_tone_release_;
  std::bitset<16> channel_env_irq_;
  std::bitset<16> channel_pitch_bend_;

  uint16_t wave_out_l_;
  uint16_t wave_out_r_;

  uint8_t main_volume_;
  uint16_t beat_base_count_;
  uint16_t current_beat_base_count_;
  union BeatCount {
    word_t raw = 0;
    Bitfield<15, 1> irq_enable;
    Bitfield<14, 1> irq_status;
    Bitfield<0, 14> beat_count;
  } beat_count_;

  union Control {
    word_t raw = 0;
    Bitfield<9, 1> no_interpolation;
    Bitfield<8, 1> low_pass_enable;
    Bitfield<6, 2> high_volume;
    Bitfield<5, 1> overflow;
    Bitfield<3, 1> init;

    static const word_t WriteMask = 0x388;
  } control_;

  BusInterface& bus_;
  Irq& irq_;
};
