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
      << std::endl
      << "  -leds            Show controller LEDs at startup" << std::endl
      << "  -fps             Show emulation FPS at startup" << std::endl;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    std::cout << "veesem (V.Smile emulator) version " VERSION << std::endl
              << "Website: http://github.com/sp1187/veesem" << std::endl
              << std::endl;

    PrintUsage(argv[0]);
    return 0;
  }
  bool read_flags = true;
  bool has_art_nvram = false;
  bool vtech_logo = true;
  bool show_leds = false;
  bool show_fps = false;
  VideoTiming video_timing = VideoTiming::PAL;
  std::string sysrom_path;
  std::string cartrom_path;
  std::string art_nvram_path;
  unsigned region_code = 0xe;  // UK English as default
  const std::vector<std::string_view> args(argv + 1, argv + argc);

  size_t argpos = 0;
  while (argpos < args.size()) {
    const auto& arg = args[argpos];
    if (read_flags && !arg.empty() && arg[0] == '-') {
      if (arg == "-sysrom") {
        if (argpos + 1 >= args.size()) {
          std::cerr << "Error: expected system ROM path" << std::endl;
          return EXIT_FAILURE;
        }
        sysrom_path = args[++argpos];
      } else if (arg == "-art-nvram") {
        if (argpos + 1 >= args.size()) {
          std::cerr << "Error: expected Art Studio NVRAM path" << std::endl;
          return EXIT_FAILURE;
        }
        art_nvram_path = args[++argpos];
      } else if (arg == "-ntsc") {
        video_timing = VideoTiming::NTSC;
      } else if (arg == "-pal") {
        video_timing = VideoTiming::PAL;
      } else if (arg == "-art") {
        has_art_nvram = true;
      } else if (arg == "-region") {
        if (argpos + 1 >= args.size()) {
          std::cerr << "Error: expected system region code" << std::endl;
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
      if (cartrom_path.empty()) {
        cartrom_path = arg;
      } else {
        std::cerr << "Error: too many ROM arguments sent" << std::endl;
        return EXIT_FAILURE;
      }
    }
    argpos++;
  }

  if (cartrom_path.empty()) {
    std::cerr << "Error: no cartridge ROM defined" << std::endl;
    return EXIT_FAILURE;
  }

  auto sysrom = std::make_unique<VSmile::SysRomType>();
  if (!sysrom_path.empty()) {
    std::ifstream sysrom_file(sysrom_path, std::ios::binary);
    if (!sysrom_file.good()) {
      std::cerr << "Error: Could not open system ROM file" << std::endl;
      return EXIT_FAILURE;
    }
    sysrom_file.read(reinterpret_cast<char*>(sysrom.get()), sizeof *sysrom);
    std::transform(sysrom->begin(), sysrom->end(), sysrom->begin(),
                   [](uint16_t x) -> uint16_t { return SDL_SwapLE16(x); });
  } else {
    // Provide game-compatible dummy system ROM if no one provided by user
    sysrom->fill(0);
    for (int i = 0xfffc0; i < 0xfffdc; i += 2) {
      (*sysrom)[i + 1] = 0x31;
    }
  }

  auto cartrom = std::make_unique<VSmile::CartRomType>();
  std::ifstream cartrom_file(cartrom_path, std::ios::binary);
  if (!cartrom_file.good()) {
    std::cerr << "Error: Could not open cartridge ROM file" << std::endl;
    return EXIT_FAILURE;
  }
  cartrom_file.read(reinterpret_cast<char*>(cartrom.get()), sizeof *cartrom);
  std::transform(cartrom->begin(), cartrom->end(), cartrom->begin(),
                 [](uint16_t x) -> uint16_t { return SDL_SwapLE16(x); });

  std::unique_ptr<VSmile::ArtNvramType> initial_art_nvram;
  if (!art_nvram_path.empty()) {
    std::ifstream art_nvram_file(art_nvram_path, std::ios::binary);
    has_art_nvram = true;
    initial_art_nvram = std::make_unique<VSmile::ArtNvramType>();
    if (art_nvram_file.good()) {
      art_nvram_file.read(reinterpret_cast<char*>(initial_art_nvram.get()),
                          sizeof *initial_art_nvram);
      std::transform(initial_art_nvram->begin(), initial_art_nvram->end(),
                     initial_art_nvram->begin(),
                     [](uint16_t x) -> uint16_t { return SDL_SwapLE16(x); });
    }
  } else if (has_art_nvram) {
    initial_art_nvram = std::make_unique<VSmile::ArtNvramType>();
  }

  return RunEmulation(std::move(sysrom), std::move(cartrom), has_art_nvram,
                      std::move(initial_art_nvram), art_nvram_path, region_code, vtech_logo,
                      video_timing, show_leds, show_fps);
}
