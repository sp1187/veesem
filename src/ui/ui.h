#pragma once

#include "core/vsmile/vsmile.h"

#include <optional>

int RunEmulation(std::unique_ptr<VSmile::SysRomType> sys_rom,
                 std::unique_ptr<VSmile::CartRomType> cart_rom, bool has_art_nvram,
                 std::optional<std::fstream> art_nvram_file, VideoTiming video_timing,
                 bool show_leds, bool show_fps);
