#include "ui.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui_impl_opengl2.h"
#include "imgui_impl_sdl2.h"
#include "imgui_stdlib.h"

#include "nfd.hpp"
#include "nfd_sdl2.h"

#include "core/vsmile/vsmile.h"
#include "graphics_state.h"

static struct UiSettings {
  bool fullscreen = false;
  bool run_emulation = true;
  bool unlock_framerate = false;
  bool on_button = false;
  bool off_button = false;
  bool restart_button = false;
  bool frame_advance = false;
  bool show_leds = false;
  bool show_fps = false;
  bool bilinear = true;
  bool show_ppu_view_settings_window = false;
  bool show_spu_output_window = false;
  bool show_load_window = false;

  std::array<float, 281250 / 4> audio_samples_left;
  std::array<float, 281250 / 4> audio_samples_right;
  int audio_samples_offset = 0;

  PpuViewSettings ppu_view_settings = {};
} ui;

static std::unique_ptr<VSmile> vsmile = nullptr;
static GraphicsState graphics_state;

static SystemConfig cur_system_config;

static void SdlAudioCallback(void* audio_stream, unsigned char* output, int len) {
  int written = SDL_AudioStreamGet(static_cast<SDL_AudioStream*>(audio_stream), output, len);
  if (written < len)
    SDL_memset(output + written, 0, len - written);
}

static VSmile::JoyInput ReadController(SDL_GameController* pad) {
  VSmile::JoyInput input;
  input.red = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X);
  input.yellow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y);
  input.blue = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
  input.green = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
  input.enter = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
  input.back = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
  input.help = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B);
  input.abc = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK);
  input.x = std::clamp(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 6144, -5, 5);
  input.y = std::clamp(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / -6144, -5, 5);

  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
    input.x = -5;
  } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
    input.x = 5;
  }
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
    input.y = -5;
  } else if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
    input.y = 5;
  }
  return input;
}

static VSmile::JoyInput ReadControllerFromKeyboard() {
  VSmile::JoyInput input;
  auto keys = SDL_GetKeyboardState(nullptr);
  input.red = keys[SDL_SCANCODE_Z];
  input.yellow = keys[SDL_SCANCODE_X];
  input.blue = keys[SDL_SCANCODE_C];
  input.green = keys[SDL_SCANCODE_V];
  input.enter = keys[SDL_SCANCODE_SPACE];
  input.help = keys[SDL_SCANCODE_A];
  input.back = keys[SDL_SCANCODE_S];

  input.abc = keys[SDL_SCANCODE_D];
  if (keys[SDL_SCANCODE_LEFT]) {
    input.x = -5;
  } else if (keys[SDL_SCANCODE_RIGHT]) {
    input.x = 5;
  }
  if (keys[SDL_SCANCODE_DOWN]) {
    input.y = -5;
  } else if (keys[SDL_SCANCODE_UP]) {
    input.y = 5;
  }

  return input;
}

static bool ImguiInit() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForOpenGL(graphics_state.GetWindow(), graphics_state.GetGlContext());
  ImGui_ImplOpenGL2_Init();

  return true;
}

static void DrawFps(bool show_number) {
  ImGuiIO& io = ImGui::GetIO();

  ImGui::SetNextWindowPos(ImVec2(4, io.DisplaySize.y - 4), ImGuiCond_Always, ImVec2(0.0, 1.0));
  ImGui::SetNextWindowBgAlpha(0.64f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
  ImGui::Begin("fps", nullptr,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_NoFocusOnAppearing);
  if (show_number) {
    ImGui::Text("FPS: %.1f", io.Framerate);
  } else {
    ImGui::Text("FPS: -");
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

static void UnloadVSmile() {
  if (!vsmile)
    return;

  // Flush Art Studio cartridge RAM to file if save path is defined
  if (cur_system_config.cart_type == VSmile::CartType::ART_STUDIO &&
      cur_system_config.csb2_nvram_save_path.has_value()) {
    std::ofstream csb2_nvram_save(*cur_system_config.csb2_nvram_save_path,
                                  std::ios::binary | std::ios::trunc);
    if (!csb2_nvram_save.good()) {
      std::cerr << "Failed to open NVRAM save file for writing" << std::endl;
    } else {
      auto final_csb2_nvram = *vsmile->GetArtNvram();
      std::transform(final_csb2_nvram.begin(), final_csb2_nvram.end(), final_csb2_nvram.begin(),
                     [](uint16_t x) -> uint16_t { return SDL_SwapLE16(x); });
      csb2_nvram_save.write(reinterpret_cast<char*>(final_csb2_nvram.data()),
                            sizeof(VSmile::ArtNvramType));
      if (csb2_nvram_save.fail()) {
        std::cerr << "Failed to write to NVRAM save file" << std::endl;
      }
    }
  }

  vsmile.reset();
}

static std::optional<std::string> LoadVSmile(const SystemConfig& config) {
  if (!config.cartrom_path.has_value()) {
    return "No cartridge ROM defined";
  }

  auto cartrom = std::make_unique<VSmile::CartRomType>();
  std::ifstream cartrom_file(*config.cartrom_path, std::ios::binary);
  if (!cartrom_file.good()) {
    return "Could not open cartridge ROM file";
  }
  cartrom_file.read(reinterpret_cast<char*>(cartrom.get()), sizeof *cartrom);
  std::transform(cartrom->begin(), cartrom->end(), cartrom->begin(),
                 [](uint16_t x) -> uint16_t { return SDL_SwapLE16(x); });

  auto sysrom = std::make_unique<VSmile::SysRomType>();
  if (config.sysrom_path.has_value()) {
    std::ifstream sysrom_file(*config.sysrom_path, std::ios::binary);
    if (!sysrom_file.good()) {
      return "Could not open system ROM file";
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

  std::unique_ptr<VSmile::ArtNvramType> initial_art_nvram;
  if (config.cart_type == VSmile::CartType::ART_STUDIO) {
    if (config.csb2_nvram_save_path.has_value()) {
      std::ifstream csb2_nvram_save_file(*config.csb2_nvram_save_path, std::ios::binary);
      initial_art_nvram = std::make_unique<VSmile::ArtNvramType>();
      if (csb2_nvram_save_file.good()) {
        csb2_nvram_save_file.read(reinterpret_cast<char*>(initial_art_nvram.get()),
                                  sizeof *initial_art_nvram);
        std::transform(initial_art_nvram->begin(), initial_art_nvram->end(),
                       initial_art_nvram->begin(),
                       [](uint16_t x) -> uint16_t { return SDL_SwapLE16(x); });
      }
    } else {
      initial_art_nvram = std::make_unique<VSmile::ArtNvramType>();
    }
  }

  if (vsmile) {
    UnloadVSmile();
  }

  vsmile = std::make_unique<VSmile>(std::move(sysrom), std::move(cartrom), config.cart_type,
                                    std::move(initial_art_nvram), config.region_code,
                                    config.vtech_logo, config.video_timing);
  vsmile->Reset();
  cur_system_config = config;

  return {};
}

static void DrawLoadWindow() {
  // TODO: insert default values from command-line arguments?

  static std::string cartrom_path = cur_system_config.cartrom_path.value_or("");
  static std::string csb2_nvram_save_path = cur_system_config.csb2_nvram_save_path.value_or("");
  static std::string sysrom_path = cur_system_config.sysrom_path.value_or("");

  static VideoTiming video_timing = cur_system_config.video_timing;
  static bool enable_csb2_nvram = cur_system_config.cart_type == VSmile::CartType::ART_STUDIO;
  static bool use_dummy_sysrom = sysrom_path.empty();
  static int region_code = cur_system_config.region_code;
  static bool vtech_logo = cur_system_config.vtech_logo;

  static std::string load_error_string;

  ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_FirstUseEver);
  ImGui::Begin("Load V.Smile ROM", &ui.show_load_window);

  ImGui::SeparatorText("Cartridge");
  ImGui::Text("Cartridge ROM path:");
  ImGui::PushItemWidth(-50);
  ImGui::InputText("##cartrom", &cartrom_path);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("...##cartrom_open")) {
    nfdu8char_t* nfd_path;
    nfdu8filteritem_t nfd_filters[] = {{"V.Smile ROM", "bin,vsmile"}};

    nfdwindowhandle_t nfd_window_handle = {NFD_WINDOW_HANDLE_TYPE_UNSET, NULL};
    NFD_GetNativeWindowFromSDLWindow(graphics_state.GetWindow(), &nfd_window_handle);

    nfdresult_t result =
        NFD::OpenDialog(nfd_path, nfd_filters, std::size(nfd_filters), nullptr, nfd_window_handle);
    if (result == NFD_OKAY) {
      cartrom_path = std::string(nfd_path);
      NFD::FreePath(nfd_path);
    }
  }

  ImGui::Dummy(ImVec2(0.0f, 15.0f));

  ImGui::Checkbox("Enable CSB2 NVRAM", &enable_csb2_nvram);
  ImGui::BeginDisabled(!enable_csb2_nvram);
  ImGui::Text("CSB2 NVRAM save:");
  ImGui::PushItemWidth(-50);
  ImGui::InputText("##csb2_nvram", &csb2_nvram_save_path);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("...##csb2_nvram_open")) {
    nfdu8char_t* nfd_path;

    nfdwindowhandle_t nfd_window_handle = {NFD_WINDOW_HANDLE_TYPE_UNSET, NULL};
    NFD_GetNativeWindowFromSDLWindow(graphics_state.GetWindow(), &nfd_window_handle);

    nfdresult_t result = NFD::SaveDialog(nfd_path, nullptr, 0, nullptr, nullptr, nfd_window_handle);
    if (result == NFD_OKAY) {
      csb2_nvram_save_path = std::string(nfd_path);
      NFD::FreePath(nfd_path);
    }
  }
  ImGui::EndDisabled();

  ImGui::SeparatorText("System");
  ImGui::RadioButton("PAL", reinterpret_cast<int*>(&video_timing),
                     static_cast<int>(VideoTiming::PAL));
  ImGui::SameLine();
  ImGui::RadioButton("NTSC", reinterpret_cast<int*>(&video_timing),
                     static_cast<int>(VideoTiming::NTSC));

  ImGui::Dummy(ImVec2(0.0f, 15.0f));

  ImGui::Checkbox("Use dummy system ROM", &use_dummy_sysrom);
  ImGui::BeginDisabled(use_dummy_sysrom);
  ImGui::Text("System ROM path:");
  ImGui::PushItemWidth(-50);
  ImGui::InputText("##sysrom", &sysrom_path);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("...##sysrom_open")) {
    {
      nfdu8char_t* nfd_path;

      nfdwindowhandle_t nfd_window_handle = {NFD_WINDOW_HANDLE_TYPE_UNSET, NULL};
      NFD_GetNativeWindowFromSDLWindow(graphics_state.GetWindow(), &nfd_window_handle);

      nfdresult_t result = NFD::OpenDialog(nfd_path, nullptr, 0, nullptr, nfd_window_handle);
      if (result == NFD_OKAY) {
        sysrom_path = std::string(nfd_path);
        NFD::FreePath(nfd_path);
      }
    }
  }
  ImGui::EndDisabled();

  ImGui::Dummy(ImVec2(0.0f, 15.0f));

  ImGui::Text("Jumpers:");
  ImGui::PushItemWidth(100);
  ImGui::SliderInt("Region code", &region_code, 0, 15, "0x%X");
  ImGui::PopItemWidth();
  ImGui::SameLine();
  ImGui::Checkbox("Display VTech logo", &vtech_logo);

  ImGui::Dummy(ImVec2(0.0f, 15.0f));
  if (ImGui::Button("Load")) {
    SystemConfig system_config;
    if (!cartrom_path.empty())
      system_config.cartrom_path = cartrom_path;
    if (!sysrom_path.empty() && !use_dummy_sysrom)
      system_config.sysrom_path = sysrom_path;
    if (!csb2_nvram_save_path.empty())
      system_config.csb2_nvram_save_path = csb2_nvram_save_path;

    system_config.cart_type =
        enable_csb2_nvram ? VSmile::CartType::ART_STUDIO : VSmile::CartType::STANDARD;
    system_config.video_timing = video_timing;
    system_config.region_code = region_code;
    system_config.vtech_logo = vtech_logo;

    auto maybe_error = LoadVSmile(system_config);
    if (!maybe_error) {
      ui.show_load_window = false;
    } else {
      load_error_string = *maybe_error;
      ImGui::OpenPopup("Load Error");
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    ui.show_load_window = false;
  }

  auto center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(load_error_string.c_str());
    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    if (ImGui::Button("OK")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}

static void DrawLeds(VSmile::JoyLedStatus leds, ImVec2 overlay_pos) {
  ImGui::SetNextWindowPos(overlay_pos, ImGuiCond_Always, ImVec2(1.0, 1.0));
  ImGui::SetNextWindowBgAlpha(0.64f);
  ImGui::SetNextWindowSize(ImVec2(112, 32));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
  ImGui::Begin("leds", nullptr,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
                   ImGuiWindowFlags_NoFocusOnAppearing);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  float x = p.x;
  float y = p.y;
  draw_list->AddCircleFilled(ImVec2(x + 12, y + 8), 10, ImColor::HSV(0, 0.6, leds.red ? 0.8 : 0.4),
                             20);
  draw_list->AddCircleFilled(ImVec2(x + 36, y + 8), 10,
                             ImColor::HSV(0.16666, 0.6, leds.yellow ? 0.8 : 0.4), 20);
  draw_list->AddCircleFilled(ImVec2(x + 60, y + 8), 10,
                             ImColor::HSV(0.66666, 0.6, leds.blue ? 0.8 : 0.4), 20);
  draw_list->AddCircleFilled(ImVec2(x + 84, y + 8), 10,
                             ImColor::HSV(0.33333, 0.6, leds.green ? 0.8 : 0.4), 20);
  ImGui::End();
  ImGui::PopStyleVar();
}

static void DrawGui() {
  ImGuiIO& io = ImGui::GetIO();
  ui.frame_advance = false;
  if ((!vsmile || (SDL_GetMouseFocus() && !ui.fullscreen)) && ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Emulation")) {
      if (ImGui::MenuItem("Load V.Smile ROM", "")) {
        ui.show_load_window = true;
      }
      ImGui::MenuItem("Run", "", &ui.run_emulation);
      ImGui::MenuItem("Unlock Framerate", "", &ui.unlock_framerate);
      ImGui::MenuItem("Frame Advance", "", &ui.frame_advance);
      ImGui::BeginDisabled(!vsmile);
      if (ImGui::MenuItem("Hard Reset")) {
        vsmile->Reset();
      }
      ImGui::EndDisabled();
      ImGui::Separator();
      ImGui::MenuItem("ON Button", "F1", &ui.on_button);
      ImGui::MenuItem("OFF Button", "F2", &ui.off_button);
      ImGui::MenuItem("RESTART Button", "F3", &ui.restart_button);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Bilinear", "", &ui.bilinear);
      if (ImGui::MenuItem("Fullscreen", "F11", &ui.fullscreen)) {
        graphics_state.SetFullscreen(ui.fullscreen);
      }
      if (ImGui::BeginMenu("Change Size")) {
        if (ImGui::MenuItem("Change to 1x")) {
          graphics_state.Resize(320, 240);
        }
        if (ImGui::MenuItem("Change to 2x")) {
          graphics_state.Resize(640, 480);
        }
        if (ImGui::MenuItem("Change to 3x")) {
          graphics_state.Resize(960, 720);
        }
        if (ImGui::MenuItem("Change to 4x")) {
          graphics_state.Resize(1280, 960);
        }
        ImGui::EndMenu();
      }
      ImGui::Separator();
      ImGui::MenuItem("Show LEDs", "", &ui.show_leds);
      ImGui::MenuItem("Show FPS", "", &ui.show_fps);
      ImGui::Separator();
      ImGui::MenuItem("Show PPU View Settings", "", &ui.show_ppu_view_settings_window);
      ImGui::MenuItem("Show SPU Output", "", &ui.show_spu_output_window);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("Website")) {
        SDL_OpenURL("http://github.com/sp1187/veesem");
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
  if (ui.show_ppu_view_settings_window) {
    ImGui::Begin("PPU View", &ui.show_ppu_view_settings_window, ImGuiWindowFlags_AlwaysAutoResize);
    bool updated = false;
    updated |= ImGui::Checkbox("Background 1", &ui.ppu_view_settings.show_bg[0]);
    updated |= ImGui::Checkbox("Background 2", &ui.ppu_view_settings.show_bg[1]);
    ImGui::Separator();
    updated |= ImGui::Checkbox("All Sprites", &ui.ppu_view_settings.show_sprites);
    if (!ui.ppu_view_settings.show_sprites) {
      ImGui::BeginDisabled();
    }
    updated |= ImGui::Checkbox("Layer 0 Sprites", &ui.ppu_view_settings.show_sprites_in_layer[0]);
    updated |= ImGui::Checkbox("Layer 1 Sprites", &ui.ppu_view_settings.show_sprites_in_layer[1]);
    updated |= ImGui::Checkbox("Layer 2 Sprites", &ui.ppu_view_settings.show_sprites_in_layer[2]);
    updated |= ImGui::Checkbox("Layer 3 Sprites", &ui.ppu_view_settings.show_sprites_in_layer[3]);
    if (!ui.ppu_view_settings.show_sprites) {
      ImGui::EndDisabled();
    }
    if (updated && vsmile) {
      vsmile->SetPpuViewSettings(ui.ppu_view_settings);  // Do this on load???
    }
    ImGui::End();
  }

  if (ui.show_spu_output_window) {
    ImGui::SetNextWindowSize(ImVec2(480, 320), ImGuiCond_FirstUseEver);
    ImGui::Begin("SPU Output", &ui.show_spu_output_window);
    static int zoom = 1;
    ImGui::SliderInt("Zoom Level", &zoom, 1, 8);
    auto region_avail = ImGui::GetContentRegionAvail();
    auto height =
        (region_avail.y - ImGui::GetStyle().ItemSpacing.y - ImGui::GetStyle().FramePadding.y) / 2;

    ImGui::PlotLines("##spu_left", std::data(ui.audio_samples_left),
                     std::size(ui.audio_samples_left), ui.audio_samples_offset, "Left Channel",
                     -(32768. / zoom), (32768. / zoom), ImVec2(region_avail.x, height));
    ImGui::PlotLines("##spu_right", std::data(ui.audio_samples_right),
                     std::size(ui.audio_samples_right), ui.audio_samples_offset, "Right Channel",
                     -(32768. / zoom), (32768. / zoom), ImVec2(region_avail.x, height));
    ImGui::End();
  }

  if (ui.show_load_window) {
    DrawLoadWindow();
  }

  if (ui.show_leds) {
    DrawLeds(vsmile ? vsmile->GetControllerLed() : VSmile::JoyLedStatus{},
             ImVec2(io.DisplaySize.x - 4, io.DisplaySize.y - 4));
  }
  if (ui.show_fps) {
    DrawFps(ui.run_emulation);
  }
}

int RunEmulation(const SystemConfig& system_config, const UiConfig& ui_config) {
  if (system_config.cartrom_path.has_value()) {
    auto load_error = LoadVSmile(system_config);
    if (load_error.has_value()) {
      std::cerr << "Load Error: " << *load_error << std::endl;
      return EXIT_FAILURE;
    }
  } else {
    // Save system config so that values from command-line arguments
    // are auto-filled in load window.

    cur_system_config = system_config;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
    std::cerr << "Platform Error: Unable to initialize SDL";
    return EXIT_FAILURE;
  }

  graphics_state.Init(640, 480);
  ImguiInit();

  NFD::Init();

  SDL_GameController* pad = nullptr;

  if (SDL_IsGameController(0)) {
    pad = SDL_GameControllerOpen(0);
    std::cout << "Controller found: " << SDL_GameControllerName(pad) << std::endl;
  };

  SDL_AudioStream* audio_stream = SDL_NewAudioStream(AUDIO_U16, 2, 281250, AUDIO_S16, 2, 48000);

  SDL_AudioSpec audiospec;
  audiospec.callback = SdlAudioCallback;
  audiospec.userdata = audio_stream;
  audiospec.freq = 48000;
  audiospec.format = AUDIO_S16;
  audiospec.channels = 2;
  audiospec.samples = 1024;

  SDL_OpenAudio(&audiospec, NULL);
  SDL_PauseAudio(0);

  SDL_Event e;
  bool quit = false;

  ui.show_leds = ui_config.show_leds;
  ui.show_fps = ui_config.show_fps;

  while (!quit) {
    while (SDL_PollEvent(&e) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&e);
      switch (e.type) {
        case SDL_QUIT:
          quit = true;
          break;
        case SDL_WINDOWEVENT:
          if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
            graphics_state.Resize(e.window.data1, e.window.data2, false);
          }
          break;
      }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      quit = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
      ui.fullscreen = !ui.fullscreen;
      graphics_state.SetFullscreen(ui.fullscreen);
    }

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    DrawGui();

    bool fast_forward = ImGui::IsKeyDown(ImGuiKey_Tab) || ui.unlock_framerate;

    if (vsmile && (ui.run_emulation || ui.frame_advance)) {
      if (pad) {
        vsmile->UpdateJoystick(ReadController(pad));
      } else {
        vsmile->UpdateJoystick(ReadControllerFromKeyboard());
      }

      vsmile->UpdateOnButton(ui.on_button || ImGui::IsKeyDown(ImGuiKey_F1));
      vsmile->UpdateOffButton(ui.off_button || ImGui::IsKeyDown(ImGuiKey_F2));
      vsmile->UpdateRestartButton(ui.restart_button || ImGui::IsKeyDown(ImGuiKey_F3));
      ui.on_button = false;
      ui.off_button = false;
      ui.restart_button = false;

      vsmile->RunFrame();

      auto ab = vsmile->GetAudio();
      SDL_AudioStreamPut(audio_stream, ab.data(), ab.size() * sizeof(uint16_t));

      if (ui.show_spu_output_window) {
        for (size_t i = 0; i < ab.size(); i += 2) {
          ui.audio_samples_left[ui.audio_samples_offset] = (ab[i] - 32768);
          ui.audio_samples_right[ui.audio_samples_offset] = (ab[i + 1] - 32768);
          ui.audio_samples_offset++;

          if (ui.audio_samples_offset == std::size(ui.audio_samples_left))
            ui.audio_samples_offset = 0;
        }
      }
    }

    if (vsmile) {
      auto fb = vsmile->GetPicture();
      graphics_state.DrawFrame(fb.data(), ui.bilinear);
    } else {
      graphics_state.ClearFrame();
    }

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    graphics_state.SwapWindow();

    while (SDL_AudioStreamAvailable(audio_stream) > 0.1 * 48000 * sizeof(int16_t)) {
      // TODO: better way to sync?
      if (fast_forward) {
        SDL_AudioStreamClear(audio_stream);
      } else {
        SDL_Delay(10);  // better way to sync????
      }
    }
    if (!vsmile || !ui.run_emulation) {
      SDL_Delay(20);
    }
  }

  UnloadVSmile();

  SDL_Quit();
  return EXIT_SUCCESS;
}
