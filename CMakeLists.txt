# X-Men Legends Launcher: an add-on for OpenXML1, built on its own.
#
#   git submodule update --init             (OpenXML1 only; not recursive)
#   cmake -S . -B build -A Win32
#   cmake --build build --config Release
#
# OpenXML1 is pinned, unmodified, in external/OpenXML1. The launcher compiles a
# few of its sources so that it reads and writes the game's files exactly as
# the game does: pc_controls.cpp (pc-settings.ini), xmlb.cpp (compiled XML
# data), the vendored miniz (the release zip) and the DX8 headers (FSAA check).
#
# 32-bit so that its FSAA check asks D3D8 exactly as the 32-bit renderer does.
cmake_minimum_required(VERSION 3.20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(XML1Launcher LANGUAGES C CXX RC)

set(XML1 "${CMAKE_CURRENT_SOURCE_DIR}/external/OpenXML1")
if(NOT EXISTS "${XML1}/src/pc_controls.cpp")
    message(FATAL_ERROR "external/OpenXML1 is missing: run git submodule update --init")
endif()

add_library(launcher-miniz STATIC
    "${XML1}/external/miniz/miniz.c" "${XML1}/external/miniz/miniz_tinfl.c"
    "${XML1}/external/miniz/miniz_tdef.c" "${XML1}/external/miniz/miniz_zip.c")
target_include_directories(launcher-miniz PUBLIC "${XML1}/external/miniz")
target_compile_definitions(launcher-miniz PUBLIC MINIZ_NO_ARCHIVE_WRITING_APIS _CRT_SECURE_NO_WARNINGS)

# Mods that ship with the launcher: every file under mods/ becomes a
# {path, contents} entry compiled into the exe (src/bundled.cpp), and the
# launcher writes them into the game's mods folder.
file(GLOB_RECURSE LAUNCHER_BUNDLED_MODS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mods/*")
set(bundled "")
foreach(source IN LISTS LAUNCHER_BUNDLED_MODS)
    file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}/mods" "${source}")
    file(READ "${source}" contents)
    string(APPEND bundled "{u8\"${relative}\", R\"BUNDLEDMOD(${contents})BUNDLEDMOD\"},\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/bundled_mods.inc.new" "${bundled}")
file(COPY_FILE "${CMAKE_CURRENT_BINARY_DIR}/bundled_mods.inc.new" "${CMAKE_CURRENT_BINARY_DIR}/bundled_mods.inc" ONLY_IF_DIFFERENT)

add_library(launcher-core STATIC
    src/mods.cpp src/text.cpp src/game.cpp src/xiso.cpp src/release.cpp src/install.cpp src/bundled.cpp
    "${XML1}/src/pc_controls.cpp" "${XML1}/src/xmlb.cpp")
target_include_directories(launcher-core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(launcher-core PUBLIC cxx_std_17)
target_compile_options(launcher-core PUBLIC /utf-8)
target_compile_definitions(launcher-core PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_include_directories(launcher-core PUBLIC src "${XML1}/src" PRIVATE "${XML1}/external/dx8-headers")
target_link_libraries(launcher-core PUBLIC launcher-miniz shell32 ole32 winhttp)

add_executable(xml1-launcher WIN32 src/main.cpp launcher.rc)
set_target_properties(xml1-launcher PROPERTIES OUTPUT_NAME "X-Men Legends Launcher")
target_link_libraries(xml1-launcher PRIVATE launcher-core comctl32 uxtheme windowscodecs)
# The manifest comes from launcher.rc.
target_link_options(xml1-launcher PRIVATE /MANIFEST:NO)

add_executable(launcher-test tests/launcher_test.cpp)
target_link_libraries(launcher-test PRIVATE launcher-core)
