#pragma once

#include <array>

struct PpuViewSettings {
  std::array<bool, 4> show_sprites_in_layer = {true, true, true, true};
  std::array<bool, 2> show_bg = {true, true};
  bool show_sprites = true;
};