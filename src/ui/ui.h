#pragma once

#include "core/vsmile/vsmile.h"

int RunEmulation(std::unique_ptr<VSmile::SysRomType> sysrom,
                 std::unique_ptr<VSmile::CartRomType> cartrom, VideoTiming video_timing,
                 bool show_leds, bool show_fps);
