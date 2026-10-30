cmake_minimum_required(VERSION 3.29)
project(Hanvi LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HANVI_IPO "Compile with IPO" ON)
option(HANVI_BUILD_DESKTOP "Build Desktop Qt application" ON)
option(HANVI_BUILD_CLI "Build CLI conversion tool" ON)
# WebAssembly builds only the core and its bindings; the browser UI in web/ is a Vite app.
if (EMSCRIPTEN)
    set(HANVI_BUILD_DESKTOP OFF)
    set(HANVI_BUILD_CLI OFF)
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

# ---- Deployment -------------------------------------------------------------
if (HANVI_APPS)
    include(Deploy)
endif ()
