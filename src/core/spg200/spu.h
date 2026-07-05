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
  Word GetWaveAddressLo(int channel_index);
  void SetWaveAddressLo(int channel_index, Word value);
  Word GetMode(int channel_index);
  void SetMode(int channel_index, Word value);
  Word GetLoopAddressLo(int channel_index);
  void SetLoopAddressLo(int channel_index, Word value);
  Word GetPan(int channel_index);
  void SetPan(int channel_index, Word value);
  Word GetEnvelope0(int channel_index);
  void SetEnvelope0(int channel_index, Word value);
  Word GetEnvelopeData(int channel_index);
  void SetEnvelopeData(int channel_index, Word value);
  Word GetEnvelope1(int channel_index);
  void SetEnvelope1(int channel_index, Word value);
  Word GetEnvelopeAddressHi(int channel_index);
  void SetEnvelopeAddressHi(int channel_index, Word value);
  Word GetEnvelopeAddressLo(int channel_index);
  void SetEnvelopeAddressLo(int channel_index, Word value);
  Word GetWaveData0(int channel_index);
  void SetWaveData0(int channel_index, Word value);
  Word GetEnvelopeLoopControl(int channel_index);
  void SetEnvelopeLoopControl(int channel_index, Word value);
  Word GetWaveData(int channel_index);
  void SetWaveData(int channel_index, Word value);

  /* 32xx values */
  Word GetPhaseHi(int channel_index);
  void SetPhaseHi(int channel_index, Word value);
  Word GetPhaseAccumulatorHi(int channel_index);
  void SetPhaseAccumulatorHi(int channel_index, Word value);
  Word GetTargetPhaseHi(int channel_index);
  void SetTargetPhaseHi(int channel_index, Word value);
  Word GetRampDownClock(int channel_index);
  void SetRampDownClock(int channel_index, Word value);
  Word GetPhaseLo(int channel_index);
  void SetPhaseLo(int channel_index, Word value);
  Word GetPhaseAccumulatorLo(int channel_index);
  void SetPhaseAccumulatorLo(int channel_index, Word value);
  Word GetTargetPhaseLo(int channel_index);
  void SetTargetPhaseLo(int channel_index, Word value);
  Word GetPitchBendControl(int channel_index);
  void SetPitchBendControl(int channel_index, Word value);

  /* 34xx values */
  Word GetChannelEnable();
  void SetChannelEnable(Word value);
  Word GetMainVolume();
  void SetMainVolume(Word value);
  Word GetChannelFiqEnable();
  void SetChannelFiqEnable(Word value);
  Word GetChannelFiqStatus();
  void ClearChannelFiqStatus(Word value);
  Word GetBeatBaseCount();
  void SetBeatBaseCount(Word value);
  Word GetBeatCount();
  void SetBeatCount(Word value);
  Word GetEnvClk0_3();
  void SetEnvClk0_3(Word value);
  Word GetEnvClk4_7();
  void SetEnvClk4_7(Word value);
  Word GetEnvClk8_11();
  void SetEnvClk8_11(Word value);
  Word GetEnvClk12_15();
  void SetEnvClk12_15(Word value);
  Word GetEnvRampdown();
  void SetEnvRampdown(Word value);
  Word GetChannelStop();
  void ClearChannelStop(Word value);
  Word GetChannelZeroCross();
  void SetChannelZeroCross(Word value);
  Word GetControl();
  void SetControl(Word value);
  Word GetCompressControl();
  void SetCompressControl(Word value);
  Word GetChannelStatus();
  void SetWaveInLeft(Word value);
  void SetWaveInRight(Word value);
  Word GetWaveOutLeft();
  void SetWaveOutLeft(Word value);
  Word GetWaveOutRight();
  void SetWaveOutRight(Word value);
  Word GetChannelRepeat();
  void SetChannelRepeat(Word value);
  Word GetChannelEnvMode();
  void SetChannelEnvMode(Word value);
  Word GetChannelToneRelease();
  void SetChannelToneRelease(Word value);
  Word GetChannelEnvIrq();
  void ClearChannelEnvIrq(Word value);
  Word GetChannelPitchBend();
  void SetChannelPitchBend(Word value);

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
    Addr wave_address = 0;
    Addr loop_address = 0;
    uint8_t wave_shift = 0;
    Addr envelope_address = 0;
    union Mode {
      Word raw = 0;
      Bitfield<15, 1> adpcm;
      Bitfield<14, 1> tone_color;
      Bitfield<12, 2> tone_mode;

      static const Word WriteMask = 0xf000;
    } mode;
    union Pan {
      Word raw = 0;
      Bitfield<8, 7> pan;
      Bitfield<0, 7> volume;

      static const Word WriteMask = 0x7f7f;
    } pan;
    union Envelope0 {
      Word raw = 0;
      Bitfield<8, 7> target;
      Bitfield<7, 1> sign;
      Bitfield<0, 7> inc;
      static const Word WriteMask = 0x7fff;
    } envelope0;
    union Envelope1 {
      Word raw = 0;
      Bitfield<9, 7> repeat_count;
      Bitfield<8, 1> repeat;
      Bitfield<0, 8> load;
    } envelope1;
    union EnvelopeIrq {
      Word raw = 0;
      Bitfield<7, 9> irq_fire_address;
      Bitfield<6, 1> irq_enable;

      static const Word WriteMask = 0xffc0;
    } envelope_irq;
    union EnvelopeData {
      Word raw = 0;
      Bitfield<8, 8> count;
      Bitfield<0, 7> edd;

      static const Word WriteMask = 0xff7f;
    } envelope_data;
    union EnvelopeLoopControl {
      Word raw = 0;
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
      Word raw = 0;
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
  std::bitset<16> channel_repeat_;
  std::bitset<16> channel_env_mode_;
  std::bitset<16> channel_tone_release_;
  std::bitset<16> channel_env_irq_;
  std::bitset<16> channel_pitch_bend_;

  uint16_t wave_out_l_;
  uint16_t wave_out_r_;
  uint16_t wave_in_l_;
  uint16_t wave_in_r_;

  uint8_t main_volume_;
  uint16_t beat_base_count_;
  uint16_t current_beat_base_count_;
  union BeatCount {
    Word raw = 0;
    Bitfield<15, 1> irq_enable;
    Bitfield<14, 1> irq_status;
    Bitfield<0, 14> beat_count;
  } beat_count_;

  union Control {
    Word raw = 0;
    Bitfield<9, 1> no_interpolation;
    Bitfield<8, 1> low_pass_enable;
    Bitfield<6, 2> high_volume;
    Bitfield<5, 1> overflow;
    Bitfield<3, 1> init;

    static const Word WriteMask = 0x388;
  } control_;

  BusInterface& bus_;
  Irq& irq_;
};
