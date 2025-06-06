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
      << "  -region NUM       Set jumpers configuring system ROM region as hex number in range 0-f"
      << std::endl
      << "  -novtech          Set jumpers disabling VTech logo in system ROM intro" << std::endl
      << std::endl
      << "  -leds            Show controller LEDs at startup" << std::endl
      << "  -fps             Show emulation FPS at startup" << std::endl
      << std::endl
      << "  -help            Print this help text" << std::endl;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    std::cout << "veesem (V.Smile emulator) version " VERSION << std::endl
              << "Website: http://github.com/sp1187/veesem" << std::endl
              << std::endl;

    PrintUsage(argv[0]);
    return EXIT_SUCCESS;
  }
  bool read_flags = true;
  VSmile::CartType cart_type = VSmile::CartType::STANDARD;
  bool vtech_logo = true;
  bool show_leds = false;
  bool show_fps = false;
  VideoTiming video_timing = VideoTiming::PAL;
  std::optional<std::string> sysrom_path;
  std::optional<std::string> cartrom_path;
  std::optional<std::string> art_nvram_path;
  unsigned region_code = 0xe;  // UK English as default
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
          std::cerr << "Error: Expected system ROM path" << std::endl;
          return EXIT_FAILURE;
        }
        sysrom_path = args[++argpos];
      } else if (arg == "-art-nvram") {
        cart_type = VSmile::CartType::ART_STUDIO;
        if (argpos + 1 >= args.size()) {
          std::cerr << "Error: Expected Art Studio NVRAM path" << std::endl;
          return EXIT_FAILURE;
        }
        art_nvram_path = args[++argpos];
      } else if (arg == "-ntsc") {
        video_timing = VideoTiming::NTSC;
      } else if (arg == "-pal") {
        video_timing = VideoTiming::PAL;
      } else if (arg == "-art") {
        cart_type = VSmile::CartType::ART_STUDIO;
      } else if (arg == "-region") {
        if (argpos + 1 >= args.size()) {
          std::cerr << "Error: Expected system region code" << std::endl;
          return EXIT_FAILURE;
        }
        const auto& num_str = args[++argpos];

        auto [ptr, error] =
            std::from_chars(num_str.data(), num_str.data() + num_str.size(), region_code, 16);

        if (ptr != (num_str.data() + num_str.size()) || error != std::errc()) {
          std::cerr << "Error: could not parse region code as hex value" << std::endl;
          return EXIT_FAILURE;
        }

        if (region_code > 0xf) {
          std::cerr << "Error: Region code out of range (should be in range 0-f)" << std::endl;
          return EXIT_FAILURE;
        }
      } else if (arg == "-novtech") {
        vtech_logo = false;
      } else if (arg == "-leds") {
        show_leds = true;
      } else if (arg == "-fps") {
        show_fps = true;
      } else if (arg == "--") {
        read_flags = false;
      } else {
        std::cerr << "Error: Unknown flag " << arg << std::endl;
        return EXIT_FAILURE;
      }
    } else {
      if (!cartrom_path.has_value()) {
        cartrom_path = arg;
      } else {
        std::cerr << "Error: too many ROM arguments sent" << std::endl;
        return EXIT_FAILURE;
      }
    }
    argpos++;
  }

  return RunEmulation(sysrom_path, cartrom_path, cart_type, art_nvram_path, region_code, vtech_logo,
                      video_timing, show_leds, show_fps);
}
