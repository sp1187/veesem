#include "vsmile_joy.h"

#include <algorithm>
#include <cassert>

#include "vsmile_common.h"

VSmileJoy::VSmileJoy(VSmileJoySend& joy_send) : joy_send_(joy_send) {}

void VSmileJoy::Reset() {
  idle_timer_.Reset();
  rts_timeout_timer_.Reset();
  tx_start_timer_.Reset();
  current_ = {};
  last_sent_ = {};
  cts_ = false;
  rts_ = true;
  tx_busy_ = false;
  joy_active_ = false;
  tx_starting_ = false;
  current_updated_ = false;
  probe_history_[0] = 0;
  probe_history_[1] = 0;
  tx_buffer_read_ = 0;
  tx_buffer_write_ = 0;
  led_status_.red = false;
  led_status_.yellow = false;
  led_status_.blue = false;
  led_status_.green = false;
}

void VSmileJoy::RunCycles(int cycles) {
  if (!tx_busy_) {
    if (idle_timer_.Tick(cycles)) {
      QueueTx(0x55);
    }
  }

  if (tx_starting_) {
    if (tx_start_timer_.Tick(cycles)) {
      tx_starting_ = false;
      StartTx();
    }
  }

  if (!rts_ && !cts_ && !tx_starting_ && !tx_busy_) {  // Waiting
    if (rts_timeout_timer_.Tick(cycles)) {
      assert(tx_buffer_read_ != tx_buffer_write_);
      assert(!tx_busy_);

      joy_send_.SetRts(true);
      rts_ = true;

      if (joy_active_) {
        current_ = {};
        current_updated_ = false;
        probe_history_[0] = 0;
        probe_history_[1] = 0;
        idle_timer_.Reset();
      }

      joy_active_ = false;

      // Reset send buffer
      tx_buffer_read_ = 0;
      tx_buffer_write_ = 0;
      tx_starting_ = false;
      QueueTx(0x55);
    }
  }

  if (joy_active_ && current_updated_) {
    QueueJoyUpdates();
  }
}

void VSmileJoy::UpdateJoystick(const JoyInput& new_input) {
  current_ = new_input;
  current_updated_ = true;
}

VSmileJoy::JoyLedStatus VSmileJoy::GetLeds() {
  return led_status_;
}

void VSmileJoy::StartTx() {
  assert(!tx_busy_);
  if (tx_buffer_read_ == tx_buffer_write_) {
    die("queue was empty");
  }
  if (tx_busy_) {
    die("uart is busy");
  }
  uint8_t byte = PopTx();
  joy_send_.Tx(byte);
  tx_busy_ = true;
}

void VSmileJoy::QueueTx(uint8_t byte) {
  const int new_write = (tx_buffer_write_ + 1) % tx_buffer_.size();
  bool was_empty = tx_buffer_read_ == tx_buffer_write_;
  if (new_write == tx_buffer_read_) {
    // Send buffer is full
    return;
  }

  tx_buffer_[tx_buffer_write_] = byte;
  tx_buffer_write_ = new_write;

  if (was_empty) {
    joy_send_.SetRts(false);
    rts_ = false;

    if (cts_) {
      if (!tx_busy_ && !tx_starting_) {
        tx_starting_ = true;
        tx_start_timer_.Reset();
      }
    } else {
      rts_timeout_timer_.Reset();
    }
  }

  idle_timer_.Reset();
}

uint8_t VSmileJoy::PopTx() {
  if (tx_buffer_write_ == tx_buffer_read_)
    die("joy: empty send buffer");

  auto value = tx_buffer_[tx_buffer_read_];
  tx_buffer_read_ = (tx_buffer_read_ + 1) % tx_buffer_.size();
  if (tx_buffer_write_ == tx_buffer_read_) {
    joy_send_.SetRts(true);
    rts_ = true;
  }

  return value;
}

void VSmileJoy::QueueJoyUpdates() {
  bool update_buttons = current_.enter != last_sent_.enter || current_.back != last_sent_.back ||
                        current_.help != last_sent_.help || current_.abc != last_sent_.abc;
  bool update_colors = current_.green != last_sent_.green || current_.blue != last_sent_.blue ||
                       current_.yellow != last_sent_.yellow || current_.red != last_sent_.red;
  bool update_joy = current_.x != last_sent_.x || current_.y != last_sent_.y;

  if (!update_buttons && !update_colors && !update_joy)
    return;

  if (update_buttons) {
    uint8_t button_value = 0xa0;
    if (current_.enter)
      button_value = 0xa1;
    else if (current_.back)
      button_value = 0xa2;
    else if (current_.help)
      button_value = 0xa3;
    else if (current_.abc)
      button_value = 0xa4;
    QueueTx(button_value);
  }
  if (update_colors) {
    uint8_t color_value =
        0x90 | current_.green | (current_.blue << 1) | (current_.yellow << 2) | (current_.red << 3);
    QueueTx(color_value);
  }
  if (update_joy) {
    uint8_t x_value = 0xc0, y_value = 0x80;
    if (current_.x != 0) {
      x_value = (current_.x < 0 ? 0xcb : 0xc3) + std::clamp(std::abs(current_.x), 1, 5) - 1;
    }
    if (current_.y != 0) {
      y_value = (current_.y < 0 ? 0x8b : 0x83) + std::clamp(std::abs(current_.y), 1, 5) - 1;
    }
    QueueTx(x_value);
    QueueTx(y_value);
  }

  idle_timer_.Reset();

  last_sent_ = current_;
  current_updated_ = false;
}

void VSmileJoy::Rx(uint8_t value) {
  switch (value & 0xf0) {
    case 0x60:
      led_status_.green = (value & 0x01) != 0;
      led_status_.blue = (value & 0x02) != 0;
      led_status_.yellow = (value & 0x04) != 0;
      led_status_.red = (value & 0x08) != 0;
      break;
    case 0x70:
    case 0xb0: {
      probe_history_[0] = ((value & 0xf0) == 0x70) ? 0 : probe_history_[1];
      probe_history_[1] = value & 0x0f;
      QueueTx(0xb0 | (((-probe_history_[0] + -probe_history_[1]) ^ 0xa) & 0xf));
    } break;
  }
}

void VSmileJoy::SetCts(bool value) {
  cts_ = value;
  if (cts_ && tx_buffer_read_ != tx_buffer_write_ && !tx_busy_ && !tx_starting_) {
    tx_starting_ = true;
    tx_start_timer_.Reset();
  }
}

void VSmileJoy::TxDone() {
  if (!tx_busy_)
    return;

  joy_active_ = true;
  tx_busy_ = false;

  if (cts_ && tx_buffer_read_ != tx_buffer_write_) {
    StartTx();
  }
}
