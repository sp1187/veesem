add_library(imgui STATIC
  imconfig.h
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_impl_opengl2.cpp
  imgui_impl_opengl2.h
  imgui_impl_sdl2.cpp
  imgui_impl_sdl2.h
  imgui_internal.h
  imgui_tables.cpp
  imgui_widgets.cpp
  imgui.cpp
  imgui.h
  imstb_rectpack.h
  imstb_textedit.h
  imstb_truetype.h
)

target_include_directories(imgui PUBLIC .)

target_link_libraries(imgui
  SDL2::SDL2
  OpenGL::OpenGL
)
