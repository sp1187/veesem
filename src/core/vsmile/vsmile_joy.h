#pragma once

#include "core/common.h"

#include <array>

class VSmileJoySend;

class VSmileJoy {
public:
  struct JoyInput {
    JoyInput() { red = yellow = blue = green = enter = back = help = abc = false; }

    int y = 0;
    int x = 0;
    unsigned red : 1;
    unsigned yellow : 1;
    unsigned blue : 1;
    unsigned green : 1;
    unsigned enter : 1;
    unsigned back : 1;
    unsigned help : 1;
    unsigned abc : 1;
  };

  struct JoyLedStatus {
    unsigned red : 1;
    unsigned yellow : 1;
    unsigned blue : 1;
    unsigned green : 1;
  };

  explicit VSmileJoy(VSmileJoySend& joy_ctrl);

  void Reset();
  void RunCycles(int cycles);
  void Rx(uint8_t value);
  void SetCts(bool value);
  void TxDone();

  void UpdateJoystick(const JoyInput& new_input);
  JoyLedStatus GetLeds();

private:
  void QueueTx(uint8_t byte);
  uint8_t PopTx();
  void QueueJoyUpdates();
  void StartTx();

  VSmileJoySend& joy_send_;
  JoyInput current_;
  JoyInput last_sent_;
  SimpleClock<27000000> idle_timer_;         // 1 s
  SimpleClock<13500000> rts_timeout_timer_;  // 0.5 s
  SimpleClock<97200> tx_start_timer_;        // 3.6 ms
  std::array<uint8_t, 16> tx_buffer_;
  int tx_buffer_write_ = 0;
  int tx_buffer_read_ = 0;
  int probe_history_[2] = {0};
  bool cts_ = false;
  bool rts_ = false;
  bool tx_busy_ = false;
  bool joy_active_ = false;
  bool tx_starting_ = false;
  bool current_updated_ = false;
  JoyLedStatus led_status_;
};
