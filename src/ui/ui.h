#pragma once

#include <optional>

#include "core/vsmile/vsmile.h"

int RunEmulation(std::optional<std::string> sysrom_path, std::optional<std::string> cartrom_path,
                 VSmile::CartType cart_type, std::optional<std::string> art_nvram_path,
                 unsigned region_code, bool vtech_logo, VideoTiming video_timing, bool show_leds,
                 bool show_fps);