#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "imgui_impl_opengl2.h"
#include "imgui_impl_sdl2.h"

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
} ui;

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

static bool ImguiInit(GraphicsState& graphics_state) {
  /* ImGui-initialisering */
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

static void DrawGui(GraphicsState& graphics_state, VSmile& vsmile) {
  ImGuiIO& io = ImGui::GetIO();
  ui.frame_advance = false;
  if (SDL_GetMouseFocus() && !ui.fullscreen && ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Emulation")) {
      ImGui::MenuItem("Run", "", &ui.run_emulation);
      ImGui::MenuItem("Unlock framerate", "", &ui.unlock_framerate);
      ImGui::MenuItem("Frame advance", "", &ui.frame_advance);
      if (ImGui::MenuItem("Hard reset")) {
        vsmile.Reset();
      }
      ImGui::Separator();
      ImGui::MenuItem("ON button", "F1", &ui.on_button);
      ImGui::MenuItem("OFF button", "F2", &ui.off_button);
      ImGui::MenuItem("RESTART button", "F3", &ui.restart_button);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Bilinear", "", &ui.bilinear);
      if (ImGui::MenuItem("Fullscreen", "F11", &ui.fullscreen)) {
        graphics_state.SetFullscreen(ui.fullscreen);
      }
      if (ImGui::BeginMenu("Change size")) {
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

  if (ui.show_leds) {
    DrawLeds(vsmile.GetControllerLed(), ImVec2(io.DisplaySize.x - 4, io.DisplaySize.y - 4));
  }
  if (ui.show_fps) {
    DrawFps(ui.run_emulation);
  }
}

int RunEmulation(std::unique_ptr<VSmile::SysRomType> sysrom,
                 std::unique_ptr<VSmile::CartRomType> cartrom, VideoTiming video_timing,
                 bool show_leds, bool show_fps) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
    std::cerr << "Unable to initialize SDL";
    return EXIT_FAILURE;
  }

  auto vsmile = std::make_unique<VSmile>(std::move(cartrom), std::move(sysrom), video_timing);
  vsmile->Reset();

  GraphicsState graphics_state;
  graphics_state.Init(640, 480);
  ImguiInit(graphics_state);

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

  ui.show_leds = show_leds;
  ui.show_fps = show_fps;

  while (!quit) {
    auto start = std::chrono::steady_clock::now();
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

    DrawGui(graphics_state, *vsmile);

    bool fast_forward = ImGui::IsKeyDown(ImGuiKey_Tab) || ui.unlock_framerate;

    if (ui.run_emulation || ui.frame_advance) {
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
      SDL_AudioStreamPut(audio_stream, ab.data(),
                         ab.size() * sizeof(uint16_t));  // separat metod i SDL-klass
    }

    auto fb = vsmile->GetPicture();
    graphics_state.DrawFrame(fb.data(), ui.bilinear);

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    graphics_state.SwapWindow();

    using frame_period = std::chrono::duration<long long, std::ratio<1, 50>>;
    auto next = start + frame_period{1};
    // std::this_thread::sleep_until(next);

    while (SDL_AudioStreamAvailable(audio_stream) > 0.1 * 48000 * sizeof(int16_t)) {
      // TODO: better way to sync?
      if (fast_forward) {
        SDL_AudioStreamClear(audio_stream);
      } else {
        SDL_Delay(10);  // better way to sync????
      }
    }
    if (!ui.run_emulation) {
      SDL_Delay(20);
    }
  }

  SDL_Quit();
  return EXIT_SUCCESS;
}
