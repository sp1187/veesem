#include "irq.h"

#include "cpu.h"

Irq::Irq(Cpu& cpu) : cpu_(cpu){};

void Irq::Reset() {
  io_irq_ctrl_.raw = 0;
  io_irq_status_.raw = 0;
  fiq_select_ = 7;
  ppu_active_ = false;
  spu_channel_active_ = false;
}

word_t Irq::GetIoIrqControl() {
  return io_irq_ctrl_.raw;
}

void Irq::SetIoIrqControl(word_t value) {
  io_irq_ctrl_.raw = value;
  UpdateIrq2();
  UpdateIrq3();
  UpdateIrq5();
  UpdateIrq6();
  UpdateIrq7();
}

word_t Irq::GetIoIrqStatus() {
  return io_irq_status_.raw;
}

void Irq::ClearIoIrqStatus(word_t value) {
  io_irq_status_.raw &= ~value;
  UpdateIrq2();
  UpdateIrq3();
  UpdateIrq5();
  UpdateIrq6();
  UpdateIrq7();
}

word_t Irq::GetFiqSelect() {
  return fiq_select_;
}

void Irq::SetFiqSelect(word_t value) {
  fiq_select_ = value & 7;
  UpdateFiq();
}

void Irq::SetPpuIrq(bool val) {
  ppu_active_ = val;
  cpu_.SetIrq(0, val);
  if (fiq_select_ == 0) {
    cpu_.SetFiq(val);
  }
}

void Irq::SetSpuChannelIrq(bool val) {
  spu_channel_active_ = val;
  cpu_.SetIrq(1, val);
  if (fiq_select_ == 1) {
    cpu_.SetFiq(val);
  }
}

void Irq::SetSpuBeatIrq(bool val) {
  cpu_.SetIrq(4, val);
}

void Irq::SetTimerAIrq(bool val) {
  io_irq_status_.timer_a = val;
  UpdateIrq2();
  if (fiq_select_ == 2) {
    UpdateFiq();
  }
}

void Irq::SetTimerBIrq(bool val) {
  io_irq_status_.timer_b = val;
  UpdateIrq2();
  if (fiq_select_ == 3) {
    UpdateFiq();
  }
}

void Irq::SetAdcIrq(bool val) {
  io_irq_status_.adc = val;
  UpdateIrq3();
  if (fiq_select_ == 6) {
    UpdateFiq();
  }
}

void Irq::SetUartIrq(bool val) {
  io_irq_status_.uart = val;
  UpdateIrq3();
  if (fiq_select_ == 4) {
    UpdateFiq();
  }
}

void Irq::SetExt1Irq(bool val) {
  io_irq_status_.ext1 = val;
  UpdateIrq5();
  if (fiq_select_ == 5) {
    UpdateFiq();
  }
}

void Irq::SetExt2Irq(bool val) {
  io_irq_status_.ext2 = val;
  UpdateIrq5();
  if (fiq_select_ == 5) {
    UpdateFiq();
  }
}

void Irq::Set1024HzIrq(bool val) {
  io_irq_status_.tick_1024hz = val;
  UpdateIrq6();
}

void Irq::Set2048HzIrq(bool val) {
  io_irq_status_.tick_2048hz = val;
  UpdateIrq6();
}

void Irq::Set4096HzIrq(bool val) {
  io_irq_status_.tick_4096hz = val;
  UpdateIrq6();
}

void Irq::SetTmb1Irq(bool val) {
  io_irq_status_.tmb1 = val;
  UpdateIrq7();
}

void Irq::SetTmb2Irq(bool val) {
  io_irq_status_.tmb2 = val;
  UpdateIrq7();
}

void Irq::Set4HzIrq(bool val) {
  io_irq_status_.tick_4hz = val;
  UpdateIrq7();
}

void Irq::UpdateIrq2() {
  auto active = GetActiveIoIrqs();
  cpu_.SetIrq(2, active.timer_a || active.timer_b);
}

void Irq::UpdateIrq3() {
  auto active = GetActiveIoIrqs();
  cpu_.SetIrq(3, active.uart || active.adc);
}

void Irq::UpdateIrq5() {
  auto active = GetActiveIoIrqs();
  cpu_.SetIrq(5, active.ext1 || active.ext2);
}

void Irq::UpdateIrq6() {
  auto active = GetActiveIoIrqs();
  cpu_.SetIrq(6, active.tick_4096hz || active.tick_2048hz || active.tick_1024hz);
}

void Irq::UpdateIrq7() {
  auto active = GetActiveIoIrqs();
  cpu_.SetIrq(7, active.key_change || active.tick_4hz || active.tmb1 || active.tmb2);
}

void Irq::UpdateFiq() {
  bool fiq_active = false;
  switch (fiq_select_) {
    case 0:
      fiq_active = ppu_active_;
      break;
    case 1:
      fiq_active = spu_channel_active_;
      break;
    case 2:
      fiq_active = GetActiveIoIrqs().timer_a;
      break;
    case 3:
      fiq_active = GetActiveIoIrqs().timer_b;
      break;
    case 4:
      fiq_active = GetActiveIoIrqs().uart;
      break;
    case 5:
      fiq_active = GetActiveIoIrqs().ext1 || GetActiveIoIrqs().ext2;
      break;
    case 6:
      fiq_active = GetActiveIoIrqs().adc;
      break;
  }
  cpu_.SetFiq(fiq_active);
}

Irq::IoInterrupts Irq::GetActiveIoIrqs() const {
  return {static_cast<word_t>(io_irq_ctrl_.raw & io_irq_status_.raw)};
}