#include "spg200.h"

#include "spg200_io.h"

Spg200::Spg200(VideoTiming video_timing, Spg200Io& io)
    : video_timing_(video_timing),
      io_(io),
      cpu_(*this),
      ppu_(video_timing, *this, irq_),
      spu_(*this, irq_),
      irq_(cpu_),
      timer_(irq_),
      extmem_(io),
      gpio_(io),
      adc_(irq_, io),
      uart_(irq_, io),
      dma_(*this) {}

void Spg200::Reset() {
  ram_.fill(0);
  cpu_.Reset();
  ppu_.Reset();
  spu_.Reset();
  irq_.Reset();
  timer_.Reset();
  extmem_.Reset();
  gpio_.Reset();
  adc_.Reset();
  uart_.Reset();
  dma_.Reset();
  random1_.Set(0x1418);
  random2_.Set(0x1658);
}

void Spg200::Step() {}

void Spg200::RunFrame() {
  for (;;) {
    int cycles = cpu_.Step();
    cycle_count_ += cycles;
    // cpu_.PrintRegisterState();

    io_.RunCycles(cycles);
    adc_.RunCycles(cycles);
    uart_.RunCycles(cycles);
    timer_.RunCycles(cycles);
    spu_.RunCycles(cycles);
    if (ppu_.RunCycles(cycles))
      break;
  }
}

std::span<uint8_t> Spg200::GetPicture() const {
  return ppu_.GetFramebuffer();
}

std::span<uint16_t> Spg200::GetAudio() {
  return spu_.GetAudio();
}

void Spg200::UartTx(uint8_t value) {
  uart_.RxStart(value);
}

void Spg200::SetExt1Irq(bool value) {
  irq_.SetExt1Irq(value);
}

void Spg200::SetExt2Irq(bool value) {
  irq_.SetExt2Irq(value);
}

word_t Spg200::ReadWord(addr_t addr) {
  addr = addr & 0x3fffff;
  switch (addr) {
    case 0 ... 0x27ff:
      return ram_[addr];
    case 0x2810:
    case 0x2816: {
      int bg_index = (addr - 0x2810) / 6;
      return ppu_.GetBgXScroll(bg_index);
    }
    case 0x2811:
    case 0x2817: {
      int bg_index = (addr - 0x2810) / 6;
      return ppu_.GetBgYScroll(bg_index);
    }
    case 0x2812:
    case 0x2818: {
      int bg_index = (addr - 0x2810) / 6;
      return ppu_.GetBgAttribute(bg_index);
    }
    case 0x2813:
    case 0x2819: {
      int bg_index = (addr - 0x2810) / 6;
      return ppu_.GetBgControl(bg_index);
    }
    case 0x2814:
    case 0x281a: {
      int bg_index = (addr - 0x2810) / 6;
      return ppu_.GetBgTileMapPtr(bg_index);
    }
    case 0x2815:
    case 0x281b: {
      int bg_index = (addr - 0x2810) / 6;
      return ppu_.GetBgAttributeMapPtr(bg_index);
    }
    case 0x281c:
      return ppu_.GetVerticalCompressAmount();
    case 0x281d:
      return ppu_.GetVerticalCompressOffset();
    case 0x2820:
    case 0x2821: {
      int bg_index = addr - 0x2820;
      return ppu_.GetBgSegmentPtr(bg_index);
    }
    case 0x2822:
      return ppu_.GetSpriteSegmentPtr();
    case 0x282a:
      return ppu_.GetBlendLevel();
    case 0x2830:
      return ppu_.GetFadeLevel();
    case 0x2836:
      return ppu_.GetIrqVpos();
    case 0x2837:
      return ppu_.GetIrqHpos();
    // case 0x2838:
    //  return ppu.GetLineCounter();
    case 0x2842:
      return ppu_.GetSpriteControl();
    case 0x2854:
      return ppu_.GetStnLcdControl();
    case 0x2862:
      return ppu_.GetIrqControl();
    case 0x2863:
      return ppu_.GetIrqStatus();
    case 0x2870:
      return ppu_.GetSpriteDmaSource();
    case 0x2871:
      return ppu_.GetSpriteDmaTarget();
    case 0x2872:
      return ppu_.GetSpriteDmaLength();
    case 0x2900 ... 0x29ff:
      return ppu_.GetLineScroll(addr & 0xff);
    case 0x2a00 ... 0x2aff:
      return ppu_.GetLineCompress(addr & 0xff);
    case 0x2b00 ... 0x2bff:
      return ppu_.GetPaletteColor(addr & 0xff);
    case 0x2c00 ... 0x2fff:
      return ppu_.ReadSpriteMemory(addr & 0x3ff);
    case 0x3000 ... 0x30ff: {
      int channel_index = (addr >> 4) & 0xf;
      switch (addr & 0xf) {
        case 0:
          return spu_.GetWaveAddressLo(channel_index);
        case 1:
          return spu_.GetMode(channel_index);
        case 2:
          return spu_.GetLoopAddressLo(channel_index);
        case 3:
          return spu_.GetPan(channel_index);
        case 4:
          return spu_.GetEnvelope0(channel_index);
        case 5:
          return spu_.GetEnvelopeData(channel_index);
        case 6:
          return spu_.GetEnvelope1(channel_index);
        case 7:
          return spu_.GetEnvelopeAddressHi(channel_index);
        case 8:
          return spu_.GetEnvelopeAddressLo(channel_index);
        case 9:
          return spu_.GetWaveData0(channel_index);
        case 10:
          return spu_.GetEnvelopeLoopControl(channel_index);
        case 11:
          return spu_.GetWaveData(channel_index);
      }
      return 0;
    }
    case 0x3200 ... 0x32ff: {
      int channel_index = (addr >> 4) & 0xf;
      switch (addr & 0xf) {
        case 0:
          return spu_.GetPhaseHi(channel_index);
        case 1:
          return spu_.GetPhaseAccumulatorHi(channel_index);
        case 2:
          return spu_.GetTargetPhaseHi(channel_index);
        case 3:
          return spu_.GetRampDownClock(channel_index);
        case 4:
          return spu_.GetPhaseLo(channel_index);
        case 5:
          return spu_.GetPhaseAccumulatorLo(channel_index);
        case 6:
          return spu_.GetTargetPhaseLo(channel_index);
        case 7:
          return spu_.GetPitchBendControl(channel_index);
      }
      return 0;
    }
    case 0x3400:
      return spu_.GetChannelEnable();
    case 0x3401:
      return spu_.GetMainVolume();
    case 0x3402:
      return spu_.GetChannelFiqEnable();
    case 0x3403:
      return spu_.GetChannelFiqStatus();
    case 0x3404:
      return spu_.GetBeatBaseCount();
    case 0x3405:
      return spu_.GetBeatCount();
    case 0x3406:
      return spu_.GetEnvClk0_3();
    case 0x3407:
      return spu_.GetEnvClk4_7();
    case 0x3408:
      return spu_.GetEnvClk8_11();
    case 0x3409:
      return spu_.GetEnvClk12_15();
    case 0x340a:
      return spu_.GetEnvRampdown();
    case 0x340b:
      return spu_.GetChannelStop();
    case 0x340c:
      return spu_.GetChannelZeroCross();
    case 0x340d:
      return spu_.GetControl();
    case 0x340f:
      return spu_.GetChannelStatus();
    case 0x3412:
      return spu_.GetWaveOutLeft();
    case 0x3413:
      return spu_.GetWaveOutRight();
    case 0x3414:
      return spu_.GetChannelRepeat();
    case 0x3415:
      return spu_.GetChannelEnvMode();
    case 0x3416:
      return spu_.GetChannelToneRelease();
    case 0x3417:
      return spu_.GetChannelEnvIrq();
    case 0x3418:
      return spu_.GetChannelPitchBend();
    case 0x3d00:
      return gpio_.GetMode();
    case 0x3d01:
    case 0x3d06:
    case 0x3d0b: {
      int port_index = (addr - 0x3d01) / 5;
      return gpio_.GetData(port_index);
    }
    case 0x3d02:
    case 0x3d07:
    case 0x3d0c: {
      int port_index = (addr - 0x3d01) / 5;
      return gpio_.GetBuffer(port_index);
    }
    case 0x3d03:
    case 0x3d08:
    case 0x3d0d: {
      int port_index = (addr - 0x3d01) / 5;
      return gpio_.GetDir(port_index);
    }
    case 0x3d04:
    case 0x3d09:
    case 0x3d0e: {
      int port_index = (addr - 0x3d01) / 5;
      return gpio_.GetAttrib(port_index);
    }
    case 0x3d05:
    case 0x3d0a:
    case 0x3d0f: {
      int port_index = (addr - 0x3d01) / 5;
      return gpio_.GetMask(port_index);
    }
    case 0x3d10:
      return timer_.GetTimebaseSetup();
    case 0x3d12:
      return timer_.GetTimerAData();
    case 0x3d13:
      return timer_.GetTimerAControl();
    case 0x3d14:
      return timer_.GetTimerAEnabled();
    case 0x3d16:
      return timer_.GetTimerBData();
    case 0x3d17:
      return timer_.GetTimerBControl();
    case 0x3d18:
      return timer_.GetTimerBEnabled();
    case 0x3d1c:
      return ppu_.GetLineCounter();
    /* 0x3d20 - System control */
    case 0x3d21:
      return irq_.GetIoIrqControl();
    case 0x3d22:
      return irq_.GetIoIrqStatus();
    case 0x3d23:
      return extmem_.GetControl();
      /* 0x3d24 - Watchdog clear */
    case 0x3d25:
      return adc_.GetControl();
    case 0x3d27:
      return adc_.GetData();
    case 0x3d2b:
      return video_timing_ == VideoTiming::PAL;
    case 0x3d2c:
      return random1_.Get();
    case 0x3d2d:
      return random2_.Get();
    case 0x3d2e:
      return irq_.GetFiqSelect();
    case 0x3d2f:
      return cpu_.GetDs();
    case 0x3d30:
      return uart_.GetControl();
    case 0x3d31:
      return uart_.GetStatus();
    case 0x3d33:
      return uart_.GetBaudLo();
    case 0x3d34:
      return uart_.GetBaudHi();
    case 0x3d35:
      return uart_.GetTx();
    case 0x3d36:
      return uart_.Rx();
    case 0x3e00:
      return dma_.GetSourceLo();
    case 0x3e01:
      return dma_.GetSourceHi();
    case 0x3e02:
      return dma_.GetLength();
    case 0x3e03:
      return dma_.GetTarget();
    case 0x4000 ... 0x3fffff:
      return extmem_.ReadWord(addr);
    default:
      return 0;
  }
};

void Spg200::WriteWord(addr_t addr, word_t value) {
  addr = addr & 0x3fffff;
  switch (addr) {
    case 0 ... 0x27ff:
      ram_[addr] = value;
      return;
    case 0x2810:
    case 0x2816: {
      int bg_index = (addr - 0x2810) / 6;
      ppu_.SetBgXScroll(bg_index, value);
      return;
    }
    case 0x2811:
    case 0x2817: {
      int bg_index = (addr - 0x2810) / 6;
      ppu_.SetBgYScroll(bg_index, value);
      return;
    }
    case 0x2812:
    case 0x2818: {
      int bg_index = (addr - 0x2810) / 6;
      ppu_.SetBgAttribute(bg_index, value);
      return;
    }
    case 0x2813:
    case 0x2819: {
      int bg_index = (addr - 0x2810) / 6;
      ppu_.SetBgControl(bg_index, value);
      return;
    }
    case 0x2814:
    case 0x281a: {
      int bg_index = (addr - 0x2810) / 6;
      ppu_.SetBgTileMapPtr(bg_index, value);
      return;
    }
    case 0x2815:
    case 0x281b: {
      int bg_index = (addr - 0x2810) / 6;
      ppu_.SetBgAttributeMapPtr(bg_index, value);
      return;
    }
    case 0x281c:
      ppu_.SetVerticalCompressAmount(value);
      return;
    case 0x281d:
      ppu_.SetVerticalCompressOffset(value);
      return;
    case 0x2820:
    case 0x2821: {
      int bg_index = addr - 0x2820;
      ppu_.SetBgSegmentPtr(bg_index, value);
      return;
    }
    case 0x2822:
      ppu_.SetSpriteSegmentPtr(value);
      return;
    case 0x282a:
      ppu_.SetBlendLevel(value);
      return;
    case 0x2830:
      ppu_.SetFadeLevel(value);
      return;
    case 0x2836:
      ppu_.SetIrqVpos(value);
      return;
    case 0x2837:
      ppu_.SetIrqHpos(value);
      return;
    case 0x2842:
      ppu_.SetSpriteControl(value);
      return;
    case 0x2854:
      ppu_.SetStnLcdControl(value);
      return;
    case 0x2862:
      ppu_.SetIrqControl(value);
      return;
    case 0x2863:
      ppu_.ClearIrqStatus(value);
      return;
    case 0x2870:
      ppu_.SetSpriteDmaSource(value);
      return;
    case 0x2871:
      ppu_.SetSpriteDmaTarget(value);
      return;
    case 0x2872:
      ppu_.StartSpriteDma(value);
      return;
    case 0x2900 ... 0x29ff:
      ppu_.SetLineScroll(addr & 0xff, value);
      return;
    case 0x2a00 ... 0x2aff:
      ppu_.SetLineCompress(addr & 0xff, value);
      return;
    case 0x2b00 ... 0x2bff:
      ppu_.SetPaletteColor(addr & 0xff, value);
      return;
    case 0x2c00 ... 0x2fff:
      ppu_.WriteSpriteMemory(addr & 0x3ff, value);
      return;
    case 0x3000 ... 0x30ff: {
      int channel = (addr >> 4) & 0xf;
      switch (addr & 0xf) {
        case 0:
          spu_.SetWaveAddressLo(channel, value);
          return;
        case 1:
          spu_.SetMode(channel, value);
          return;
        case 2:
          spu_.SetLoopAddressLo(channel, value);
          return;
        case 3:
          spu_.SetPan(channel, value);
          return;
        case 4:
          spu_.SetEnvelope0(channel, value);
          return;
        case 5:
          spu_.SetEnvelopeData(channel, value);
          return;
        case 6:
          spu_.SetEnvelope1(channel, value);
          return;
        case 7:
          spu_.SetEnvelopeAddressHi(channel, value);
          return;
        case 8:
          spu_.SetEnvelopeAddressLo(channel, value);
          return;
        case 9:
          spu_.SetWaveData0(channel, value);
          return;
        case 10:
          spu_.SetEnvelopeLoopControl(channel, value);
          return;
        case 11:
          spu_.SetWaveData(channel, value);
          return;
      }
    }
      return;
    case 0x3200 ... 0x32ff: {
      int channel = (addr >> 4) & 0xf;
      switch (addr & 0xf) {
        case 0:
          spu_.SetPhaseHi(channel, value);
          return;
        case 1:
          spu_.SetPhaseAccumulatorHi(channel, value);
          return;
        case 2:
          spu_.SetTargetPhaseHi(channel, value);
          return;
        case 3:
          spu_.SetRampDownClock(channel, value);
          return;
        case 4:
          spu_.SetPhaseLo(channel, value);
          return;
        case 5:
          spu_.SetPhaseAccumulatorLo(channel, value);
          return;
        case 6:
          spu_.SetTargetPhaseLo(channel, value);
          return;
        case 7:
          spu_.SetPitchBendControl(channel, value);
          return;
      }
    }
      return;
    case 0x3400:
      spu_.SetChannelEnable(value);
      return;
    case 0x3401:
      spu_.SetMainVolume(value);
      return;
    case 0x3402:
      spu_.SetChannelFiqEnable(value);
      return;
    case 0x3403:
      spu_.ClearChannelFiqStatus(value);
      return;
    case 0x3404:
      spu_.SetBeatBaseCount(value);
      return;
    case 0x3405:
      spu_.SetBeatCount(value);
      return;
    case 0x3406:
      spu_.SetEnvClk0_3(value);
      return;
    case 0x3407:
      spu_.SetEnvClk4_7(value);
      return;
    case 0x3408:
      spu_.SetEnvClk8_11(value);
      return;
    case 0x3409:
      spu_.SetEnvClk12_15(value);
      return;
    case 0x340a:
      spu_.SetEnvRampdown(value);
      return;
    case 0x340b:
      spu_.ClearChannelStop(value);
      return;
    case 0x340c:
      spu_.SetChannelZeroCross(value);
      return;
    case 0x340d:
      spu_.SetControl(value);
      return;
    case 0x3410:
      spu_.SetWaveInLeft(value);
      return;
    case 0x3411:
      spu_.SetWaveInRight(value);
      return;
    case 0x3414:
      spu_.SetChannelRepeat(value);
      return;
    case 0x3415:
      spu_.SetChannelEnvMode(value);
      return;
    case 0x3416:
      spu_.SetChannelToneRelease(value);
      return;
    case 0x3417:
      spu_.ClearChannelEnvIrq(value);
      return;
    case 0x3418:
      spu_.SetChannelPitchBend(value);
      return;
    case 0x3d00:
      gpio_.SetMode(value);
      return;
    case 0x3d01:
    case 0x3d02:
    case 0x3d06:
    case 0x3d07:
    case 0x3d0b:
    case 0x3d0c: {
      int port_index = (addr - 0x3d01) / 5;
      gpio_.SetBuffer(port_index, value);
      return;
    }
    case 0x3d03:
    case 0x3d08:
    case 0x3d0d: {
      int port_index = (addr - 0x3d01) / 5;
      gpio_.SetDir(port_index, value);
      return;
    }
    case 0x3d04:
    case 0x3d09:
    case 0x3d0e: {
      int port_index = (addr - 0x3d01) / 5;
      gpio_.SetAttrib(port_index, value);
      return;
    }
    case 0x3d05:
    case 0x3d0a:
    case 0x3d0f: {
      int port_index = (addr - 0x3d01) / 5;
      gpio_.SetMask(port_index, value);
      return;
    }
    case 0x3d10:
      timer_.SetTimebaseSetup(value);
      return;
    case 0x3d11:
      timer_.ClearTimebaseCounter();
      return;
    case 0x3d12:
      timer_.SetTimerAData(value);
      return;
    case 0x3d13:
      timer_.SetTimerAControl(value);
      return;
    case 0x3d14:
      timer_.SetTimerAEnabled(value);
      return;
    case 0x3d15:
      timer_.ClearTimerAIrq();
      return;
    case 0x3d16:
      timer_.SetTimerBData(value);
      return;
    case 0x3d17:
      timer_.SetTimerBControl(value);
      return;
    case 0x3d18:
      timer_.SetTimerBEnabled(value);
      return;
    case 0x3d19:
      timer_.ClearTimerBIrq();
      return;
    /* 0x3d20 - System control */
    case 0x3d21:
      irq_.SetIoIrqControl(value);
      return;
    case 0x3d22:
      irq_.ClearIoIrqStatus(value);
      return;
    case 0x3d23:
      extmem_.SetControl(value);
      return;
    /* 0x3d24 - Watchdog clear */
    case 0x3d25:
      adc_.SetControl(value);
      return;
    /* 0x3d28...0x3d2a - Sleep/wakeup */
    case 0x3d2c:
      random1_.Set(value);
      return;
    case 0x3d2d:
      random2_.Set(value);
      return;
    case 0x3d2e:
      irq_.SetFiqSelect(value);
      return;
    case 0x3d2f:
      cpu_.SetDs(value);
      return;
    case 0x3d30:
      uart_.SetControl(value);
      return;
    case 0x3d31:
      uart_.SetStatus(value);
      return;
    case 0x3d32:
      uart_.SoftReset();
      return;
    case 0x3d33:
      uart_.SetBaudLo(value);
      return;
    case 0x3d34:
      uart_.SetBaudHi(value);
      return;
    case 0x3d35:
      uart_.Tx(value);
      return;
    case 0x3e00:
      dma_.SetSourceLo(value);
      return;
    case 0x3e01:
      dma_.SetSourceHi(value);
      return;
    case 0x3e02:
      dma_.StartDma(value);
      return;
    case 0x3e03:
      dma_.SetTarget(value);
      return;
    case 0x4000 ... 0x3fffff:
      extmem_.WriteWord(addr, value);
    default:
      return;  // ignore writes
  }
};
