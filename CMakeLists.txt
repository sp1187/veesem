cmake_minimum_required (VERSION 3.14)

project (veesem CXX)
include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_compile_options(
  -Wall 
  -Wextra 
  -Wno-unused-parameter
  -fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.
  $<$<CONFIG:Debug>:-Og> 
)

#add_compile_options(-fsanitize=undefined)
#add_link_options(-fsanitize=undefined)

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

#add_link_options(-pg)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

add_subdirectory(src)

if(CMAKE_SYSTEM_NAME MATCHES "Linux|FreeBSD")
  install(FILES resources/veesem.svg
          DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps)
  install(FILES resources/veesem.desktop
          DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
endif()
