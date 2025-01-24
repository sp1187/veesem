#pragma once

#include "adc.h"
#include "bus_interface.h"
#include "core/common.h"
#include "cpu.h"
#include "dma.h"
#include "extmem.h"
#include "gpio.h"
#include "irq.h"
#include "ppu.h"
#include "random.h"
#include "settings.h"
#include "spu.h"
#include "timer.h"
#include "types.h"
#include "uart.h"

class Spg200Io;

class Spg200 : public BusInterface {
public:
  Spg200(VideoTiming video_timing, Spg200Io& io);
  ~Spg200() = default;

  void RunFrame();
  void Step();
  void Reset();

  void UartTx(uint8_t value);
  void SetExt1Irq(bool value);
  void SetExt2Irq(bool value);

  std::span<uint8_t> GetPicture() const;
  std::span<uint16_t> GetAudio();

  void SetPpuViewSettings(PpuViewSettings& ppu_view_settings);

  // BusInterface
  word_t ReadWord(addr_t addr) override;
  void WriteWord(addr_t addr, word_t val) override;

private:
  uint64_t cycle_count_ = 0;

  const VideoTiming video_timing_;
  Spg200Io& io_;

  std::array<uint16_t, 0x2800> ram_ = {0};
  Cpu cpu_;
  Ppu ppu_;
  Spu spu_;
  Irq irq_;
  Timer timer_;
  Extmem extmem_;
  Gpio gpio_;
  Adc adc_;
  Uart uart_;
  Dma dma_;
  Random random1_;
  Random random2_;
};