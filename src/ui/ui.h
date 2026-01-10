#pragma once

#include <optional>

#include "core/vsmile/vsmile.h"

struct SystemConfig {
  std::optional<std::string> sysrom_path;
  std::optional<std::string> cartrom_path;
  std::optional<std::string> csb2_nvram_save_path;
  VSmile::CartType cart_type = VSmile::CartType::STANDARD;
  VideoTiming video_timing = VideoTiming::PAL;
  unsigned region_code = 0xe;
  bool vtech_logo = true;
};

struct UiConfig {
  bool show_leds = false;
  bool show_fps = false;
};

int RunEmulation(const SystemConfig& system_config, const UiConfig& ui_config);