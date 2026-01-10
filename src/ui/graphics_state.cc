
#include "graphics_state.h"

void GraphicsState::Init(int width, int height) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  window_ = SDL_CreateWindow("veesem", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width,
                             height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  gl_context_ = SDL_GL_CreateContext(window_);
  SDL_GL_MakeCurrent(window_, gl_context_);

  // GL settings
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  glEnable(GL_TEXTURE_2D);

  // bind texture
  glGenTextures(1, &texture_id_);

  // set size of frame buffer
  Resize(width, height);
}

void GraphicsState::Resize(int width, int height, bool resize_window) {
  if (resize_window) {
    SDL_SetWindowSize(window_, width, height);
  }
  glViewport(0, 0, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, 1, -1);

  window_width_ = width;
  window_height_ = height;

  float prop = ((float)width) / height;
  if (prop > 4.0 / 3) {
    margin_width_ = (width - height * (4.0 / 3)) / 2;
    margin_height_ = 0;
  } else {
    margin_width_ = 0;
    margin_height_ = (height - width / (4.0 / 3)) / 2;
  }
}

void GraphicsState::DrawFrame(uint8_t* fb, bool bilinear) {
  ClearFrame();

  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5, 320, 240, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, fb);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(margin_width_, margin_height_);
  glTexCoord2f(1, 0);
  glVertex2f(window_width_ - margin_width_, margin_height_);
  glTexCoord2f(1, 1);
  glVertex2f(window_width_ - margin_width_, window_height_ - margin_height_);
  glTexCoord2f(0, 1);
  glVertex2f(margin_width_, window_height_ - margin_height_);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GraphicsState::ClearFrame() {
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
}

void GraphicsState::SwapWindow() {
  SDL_GL_SwapWindow(window_);
}

SDL_Window* GraphicsState::GetWindow() {
  return window_;
}

SDL_GLContext GraphicsState::GetGlContext() {
  return gl_context_;
}

void GraphicsState::SetFullscreen(bool fullscreen) {
  SDL_SetWindowFullscreen(window_, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}