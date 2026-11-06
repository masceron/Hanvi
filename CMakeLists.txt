cmake_minimum_required(VERSION 3.29)
project(Hanvi LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HANVI_IPO "Compile with IPO" ON)
option(HANVI_BUILD_DESKTOP "Build Desktop Qt application" ON)
option(HANVI_BUILD_CLI "Build CLI conversion tool" ON)
option(HANVI_BUILD_BENCH "Build the conversion benchmark (bench/), which needs no Qt" OFF)
option(HANVI_PGO "Clang Release builds: optimize with a profile trained on train/*.txt, when there are any" ON)
set(HANVI_PGO_TRAINING "" CACHE PATH "Texts to train a PGO profile on; makes this an instrumented build of the benchmark only")
# An instrumented build only runs the benchmark, to write the profile.
if (HANVI_PGO_TRAINING)
    set(HANVI_BUILD_DESKTOP OFF)
    set(HANVI_BUILD_CLI OFF)
    set(HANVI_BUILD_BENCH ON)
endif ()
# WebAssembly builds only the core and its bindings; the browser UI in web/ is a Vite app.
if (EMSCRIPTEN)
    set(HANVI_BUILD_DESKTOP OFF)
    set(HANVI_BUILD_CLI OFF)
    set(HANVI_BUILD_BENCH OFF)
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)

if (HANVI_BUILD_DESKTOP OR HANVI_BUILD_CLI)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
endif ()

# ---- Core Logic -------------------------------------------------------------
add_subdirectory(core)

# ---- Ports / Frontends ------------------------------------------------------
set(HANVI_APPS "")
if (HANVI_BUILD_DESKTOP)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    add_subdirectory(desktop)
    list(APPEND HANVI_APPS Hanvi)
endif ()

if (HANVI_BUILD_CLI)
    add_subdirectory(cli)
    list(APPEND HANVI_APPS HanviCLI)
endif ()

if (EMSCRIPTEN)
    add_subdirectory(web/wasm)
endif ()

if (HANVI_BUILD_BENCH)
    add_subdirectory(bench)
endif ()

# ---- Deployment -------------------------------------------------------------
if (HANVI_APPS)
    include(Deploy)
endif ()
