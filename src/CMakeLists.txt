set(OpenGL_GL_PREFERENCE GLVND)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_subdirectory(contrib/imgui)

SET(NFD_PORTAL ON CACHE BOOL "Use dbus for native file dialog instead of gtk")
add_subdirectory(contrib/nativefiledialog-extended)

add_library(veesem_core STATIC
  core/common.h
  core/spg200/adc.cc
  core/spg200/adc.h
  core/spg200/adpcm.cc
  core/spg200/adpcm.h
  core/spg200/bus_interface.h
  core/spg200/cpu.cc
  core/spg200/cpu.h
  core/spg200/dma.cc
  core/spg200/dma.h
  core/spg200/extmem.cc
  core/spg200/extmem.h
  core/spg200/gpio.cc
  core/spg200/gpio.h
  core/spg200/irq.cc
  core/spg200/irq.h
  core/spg200/ppu.cc
  core/spg200/ppu.h
  core/spg200/random.cc
  core/spg200/random.h 
  core/spg200/spg200.cc
  core/spg200/spg200.h
  core/spg200/spg200_io.h
  core/spg200/spu.cc
  core/spg200/spu.h
  core/spg200/timer.cc
  core/spg200/timer.h
  core/spg200/types.h
  core/spg200/uart.cc
  core/spg200/uart.h
  core/vsmile/vsmile.cc
  core/vsmile/vsmile.h
  core/vsmile/vsmile_common.h
  core/vsmile/vsmile_joy.cc
  core/vsmile/vsmile_joy.h
)

target_include_directories(veesem_core PUBLIC .)

add_library(veesem_ui STATIC
  ui/graphics_state.cc
  ui/graphics_state.h
  ui/ui.cc
  ui/ui.h
)

target_link_libraries(veesem_ui
  veesem_core
  imgui
  nfd
  SDL2::SDL2
  OpenGL::GL
)
target_include_directories(veesem_ui PUBLIC .)

add_executable(veesem main.cc)
target_include_directories(veesem PUBLIC .)
target_link_libraries(veesem
  veesem_ui
)

install(TARGETS veesem DESTINATION bin)
