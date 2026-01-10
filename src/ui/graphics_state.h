#pragma once

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdint.h>

class GraphicsState {
public:
  void Init(int width, int height);
  void Resize(int width, int height, bool resize_window = true);
  void DrawFrame(uint8_t* fb, bool bilinear = false);
  void ClearFrame();
  void SwapWindow();
  SDL_Window* GetWindow();
  SDL_GLContext GetGlContext();
  void SetFullscreen(bool fullscreen);

private:
  GLuint texture_id_ = 0;
  SDL_Window* window_ = nullptr;
  SDL_GLContext gl_context_;
  int window_width_ = 0;
  int window_height_ = 0;
  int margin_width_ = 0;
  int margin_height_ = 0;
};
