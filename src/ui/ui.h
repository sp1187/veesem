#pragma once

#include "core/vsmile/vsmile.h"

int RunEmulation(std::unique_ptr<VSmile::SysRomType> sys_rom,
                 std::unique_ptr<VSmile::CartRomType> cart_rom, bool has_art_ram,
                 VideoTiming video_timing, bool show_leds, bool show_fps);
