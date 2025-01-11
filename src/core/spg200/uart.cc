#include "uart.h"

#include "irq.h"
#include "spg200_io.h"

Uart::Uart(Irq& irq, Spg200Io& io) : irq_(irq), io_(io) {}

void Uart::Reset() {
  control_.raw = 0;
  status_.raw = 0;
  status_.bit9 = 1;
  baud_lo_ = 0;
  baud_hi_ = 0;

  tx_buf_ = 0;
  tx_running_ = 0;
  rx_buf_ = 0;
  rx_running_ = 0;

  rx_counter_ = 0;
  tx_counter_ = 0;
}

void Uart::RunCycles(int cycles) {
  if (tx_counter_) {
    tx_counter_ -= cycles;

    if (tx_counter_ <= 0) {
      tx_counter_ = 0;
      status_.tx_ready = true;
      status_.tx_busy = false;

      io_.TxUart(tx_running_);

      bool rx_irq_active = control_.rx_irq_enable && status_.rx_ready;
      bool tx_irq_active = control_.tx_irq_enable && status_.tx_ready;

      irq_.SetUartIrq(rx_irq_active || tx_irq_active);
    }
  }

  if (rx_counter_) {
    rx_counter_ -= cycles;
    if (rx_counter_ <= 0) {
      rx_counter_ = 0;
      status_.rx_full = true;
      status_.rx_ready = true;

      rx_buf_ = rx_running_;

      io_.RxUartDone();

      bool rx_irq_active = control_.rx_irq_enable && status_.rx_ready;
      bool tx_irq_active = control_.tx_irq_enable && status_.tx_ready;

      irq_.SetUartIrq(rx_irq_active || tx_irq_active);
    }
  }
}

word_t Uart::GetControl() {
  return control_.raw;
}

void Uart::SetControl(word_t value) {
  bool old_tx_enable = control_.tx_enable;
  control_.raw = value;

  if (!control_.rx_enable) {
    rx_buf_ = 0;
  }

  bool rx_irq_active = control_.rx_irq_enable && status_.rx_ready;
  bool tx_irq_active = control_.tx_irq_enable && status_.tx_ready;

  irq_.SetUartIrq(rx_irq_active || tx_irq_active);

  if (control_.tx_enable != old_tx_enable) {
    status_.tx_ready = control_.tx_enable;
    if (!control_.tx_enable) {
      status_.tx_busy = false;
      tx_counter_ = 0;
    }
  }
}

word_t Uart::GetStatus() {
  return status_.raw;
}

void Uart::SetStatus(word_t value) {
  status_.raw &= ~(value & UartStatus::ClearMask);

  bool rx_irq_active = control_.rx_irq_enable && status_.rx_ready;
  bool tx_irq_active = control_.tx_irq_enable && status_.tx_ready;

  irq_.SetUartIrq(rx_irq_active || tx_irq_active);
}

void Uart::SoftReset() {
  // TODO: what needs to be reset here?
}

word_t Uart::GetBaudLo() {
  return baud_lo_;
}

void Uart::SetBaudLo(word_t value) {
  baud_lo_ = value & 0xFF;
}

word_t Uart::GetBaudHi() {
  return baud_hi_;
}

void Uart::SetBaudHi(word_t value) {
  baud_hi_ = value & 0xFF;
}

word_t Uart::GetTx() {
  return tx_buf_;
}

void Uart::Tx(word_t value) {
  tx_buf_ = value;

  if (control_.tx_enable && !status_.tx_busy) {
    tx_running_ = value;
    status_.tx_ready = false;
    status_.tx_busy = true;

    const unsigned baud = (baud_hi_ << 8) | baud_lo_;
    tx_counter_ = 16 * (65536 - baud) * (control_.mode ? 11 : 10);
  }
}

word_t Uart::Rx() {
  status_.rx_full = false;

  return rx_buf_;
}

void Uart::RxStart(uint8_t value) {
  if (rx_counter_)
    return;
  if (!control_.rx_enable)
    return;

  const unsigned baud = (baud_hi_ << 8) | baud_lo_;
  rx_counter_ = 16 * (65536 - baud) * (control_.mode ? 11 : 10);
  rx_running_ = value;
}