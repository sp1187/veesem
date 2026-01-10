#include <algorithm>
#include <charconv>
#include <iostream>
#include <vector>

#include <SDL_endian.h>

#include "core/spg200/types.h"
#include "ui/ui.h"

#define VERSION "0.1"

void PrintUsage(std::string exec_name) {
  std::cout
      << "veesem (V.Smile emulator) version " VERSION << std::endl
      << "Website: http://github.com/sp1187/veesem" << std::endl
      << std::endl
      << "Usage: " << exec_name << " [OPTIONS] CARTROM" << std::endl
      << std::endl
      << "Options:" << std::endl
      << "  -sysrom ROM       Provide system ROM" << std::endl
      << "  -pal              Use PAL video timing (default)" << std::endl
      << "  -ntsc             Use NTSC video timing" << std::endl
      << "  -art              Emulate CSB2 cartridge NVRAM (used by V.Smile Art Studio)"
      << std::endl
      << "  -art-nvram FILE   Emulate CSB2 cartridge NVRAM and use FILE for persistent saving"
      << std::endl
      << "  -region NUM       Set jumpers configuring system ROM region as hex number in range "
         "0-f"
      << std::endl
      << "  -novtech          Set jumpers disabling VTech logo in system ROM intro" << std::endl
      << std::endl
      << "  -leds            Show controller LEDs at startup" << std::endl
      << "  -fps             Show emulation FPS at startup" << std::endl
      << std::endl
      << "  -help            Print this help text" << std::endl;
}

int main(int argc, char** argv) {
  SystemConfig system_config;
  system_config.cart_type = VSmile::CartType::STANDARD;
  system_config.region_code = 0xe;  // UK English as default
  system_config.vtech_logo = true;
  system_config.video_timing = VideoTiming::PAL;

  UiConfig ui_config;
  ui_config.show_leds = false;
  ui_config.show_fps = false;

  bool read_flags = true;
  const std::vector<std::string_view> args(argv + 1, argv + argc);

  size_t argpos = 0;
  while (argpos < args.size()) {
    const auto& arg = args[argpos];
    if (read_flags && !arg.empty() && arg[0] == '-') {
      if (arg == "-help" || arg == "--help") {
        PrintUsage(argv[0]);
        return EXIT_SUCCESS;
      }
      if (arg == "-sysrom") {
        if (argpos + 1 >= args.size()) {
          std::cerr << "Argument error: Expected system ROM path" << std::endl;
          return EXIT_FAILURE;
        }
        system_config.sysrom_path = args[++argpos];
      } else if (arg == "-art-nvram") {
        system_config.cart_type = VSmile::CartType::ART_STUDIO;
        if (argpos + 1 >= args.size()) {
          std::cerr << "Argument error: Expected Art Studio NVRAM path" << std::endl;
          return EXIT_FAILURE;
        }
        system_config.csb2_nvram_save_path = args[++argpos];
      } else if (arg == "-ntsc") {
        system_config.video_timing = VideoTiming::NTSC;
      } else if (arg == "-pal") {
        system_config.video_timing = VideoTiming::PAL;
      } else if (arg == "-art") {
        system_config.cart_type = VSmile::CartType::ART_STUDIO;
      } else if (arg == "-region") {
        if (argpos + 1 >= args.size()) {
          std::cerr << "Error: Expected system region code" << std::endl;
          return EXIT_FAILURE;
        }
        const auto& num_str = args[++argpos];

        auto [ptr, error] = std::from_chars(num_str.data(), num_str.data() + num_str.size(),
                                            system_config.region_code, 16);

        if (ptr != (num_str.data() + num_str.size()) || error != std::errc()) {
          std::cerr << "Argument error: could not parse region code as hex value" << std::endl;
          return EXIT_FAILURE;
        }

        if (system_config.region_code > 0xf) {
          std::cerr << "Argument error: Region code out of range (should be in range 0-f)"
                    << std::endl;
          return EXIT_FAILURE;
        }
      } else if (arg == "-novtech") {
        system_config.vtech_logo = false;
      } else if (arg == "-leds") {
        ui_config.show_leds = true;
      } else if (arg == "-fps") {
        ui_config.show_fps = true;
      } else if (arg == "--") {
        read_flags = false;
      } else {
        std::cerr << "Argument error: Unknown flag " << arg << std::endl;
        return EXIT_FAILURE;
      }
    } else {
      if (!system_config.cartrom_path.has_value()) {
        system_config.cartrom_path = arg;
      } else {
        std::cerr << "Argument error: too many ROM arguments sent" << std::endl;
        return EXIT_FAILURE;
      }
    }
    argpos++;
  }

  return RunEmulation(system_config, ui_config);
}
