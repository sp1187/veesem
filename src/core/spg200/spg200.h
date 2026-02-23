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
#include "watchdog.h"

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

  // Read variant without side effects, used for memory editor
  word_t PeekWord(addr_t addr);

private:
  word_t GetSystemControl();
  void SetSystemControl(word_t value);

  const VideoTiming video_timing_;
  Spg200Io& io_;

  std::array<uint16_t, 0x2800> ram_ = {0};
  union SystemControl {
    word_t raw = 0;
    Bitfield<15, 1> watchdog_enable;
    Bitfield<14, 1> sleep_enable;
    Bitfield<9, 1> lvr_output_enable;
    Bitfield<8, 1> lvr_enable;
    Bitfield<7, 1> two_volt_disable;
    Bitfield<5, 2> lvd_voltage_select;
    Bitfield<4, 1> timer_clock_disable;
    Bitfield<2, 1> video_dac_disable;
    Bitfield<1, 1> audio_dac_disable;

    static const word_t WriteMask = 0xc3f6;
  } system_ctrl_;
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
  Watchdog watchdog_;
};