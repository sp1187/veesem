#include "adc.h"

#include "irq.h"
#include "spg200_io.h"

Adc::Adc(Irq& irq, Spg200Io& io) : irq_(irq), io_(io) {}

void Adc::Reset() {
  adc_clock_.Reset();
  active_channel_ = -1;

  ctrl_.raw = 0;
  status_.raw = 0;
  data_.raw = 0;
}

void Adc::RunCycles(int cycles) {
  if (active_channel_ >= 0) {
    if (adc_clock_.Tick(cycles) && adc_clock_.GetDividedTick(ctrl_.clock)) {
      switch (active_channel_) {
        case 0:
          data_.data = io_.GetAdc0();
          break;
        case 1:
          data_.data = io_.GetAdc1();
          break;
        case 2:
          data_.data = io_.GetAdc2();
          break;
        case 3:
          data_.data = io_.GetAdc3();
          break;
      }
      data_.ready = true;
      status_.irq = true;
      active_channel_ = -1;
      if (ctrl_.int_enable) {
        irq_.SetAdcIrq(true);
      }
    }
  }
}

void Adc::SetControl(uint16_t value) {
  ctrl_.raw = value & AdcControl::WriteMask;
  status_.raw &= ~(value & AdcControlStatus::WriteMask);

  if (ctrl_.req_auto_8k) {
    die("unexpected req auto 8k");
  }

  if (!status_.irq) {
    irq_.SetAdcIrq(false);
  }

  if (ctrl_.enabled) {
    status_.irq = true;

    if (ctrl_.request) {
      status_.irq = false;
      ctrl_.request = false;

      active_channel_ = ctrl_.channel;
      data_.ready = false;
    }
  } else {
    active_channel_ = -1;
  }
}

word_t Adc::GetControl() {
  return ctrl_.raw | status_.raw;
}
word_t Adc::GetData() {
  return data_.raw;
}