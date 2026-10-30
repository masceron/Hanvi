cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _compiler_dir)

# ---- WebAssembly exceptions -------------------------------------------------
# Emscripten turns C++ exception catching off by default, but the core and
# OpenCC rely on it (e.g. std::async falling back when threads are missing).
if (EMSCRIPTEN)
    add_compile_options(-fwasm-exceptions)
    add_link_options(-fwasm-exceptions)
endif ()

# ---- Linker ------------------------------------------------------------------
# Clang on Linux defaults to GNU ld, which supports neither Clang LTO (without
# the gold plugin) nor --icf; llvm-mingw already defaults to lld.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
    find_program(LLD_EXECUTABLE ld.lld HINTS "${_compiler_dir}" REQUIRED)
    set(CMAKE_LINKER_TYPE LLD)
endif ()

# ---- IPO ---------------------------------------------------------------------
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if (NOT ipo_supported)
    message(WARNING "IPO is not supported: ${ipo_error}")
    set(HANVI_IPO OFF)
endif ()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${HANVI_IPO})

# ---- Project-only flags ------------------------------------------------------
add_library(hanvi_flags INTERFACE)
if (MSVC)
    set(_warn         /W3 /WX /permissive- /utf-8)
    set(_release      /Ob3 /Oi /Ot /GF /GA /Gy /Gw)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        list(APPEND _release /Qpar)
    endif ()
    set(_release_link /OPT:REF /OPT:ICF)
    target_compile_definitions(hanvi_flags INTERFACE NOMINMAX)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_warn         -Wall -Wextra -Werror -pedantic-errors)
    set(_release      -fomit-frame-pointer -ffunction-sections -fdata-sections -fipa-pta)
    # Qt requires -fPIC; without it at the LTO link step GCC emits PIE code
    # with copy relocations against Qt data and the app crashes on startup.
    set(_release_link -s -Wl,--gc-sections -flto-partition=one -fPIC)
elseif (EMSCRIPTEN)
    # emcc already strips and garbage-collects sections in optimized builds;
    # its `-s` means a setting, not strip.
    set(_warn         -Wall -Wextra -Werror -pedantic-errors)
    set(_release      "")
    set(_release_link "")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(_warn         -Wall -Wextra -Werror -pedantic-errors)
    set(_release      -fomit-frame-pointer -ffunction-sections -fdata-sections)
    set(_release_link -s -Wl,--gc-sections -Wl,--icf=all)
else ()
    message(FATAL_ERROR "Compiler not supported.")
endif ()

target_compile_options(hanvi_flags INTERFACE ${_warn} "$<$<CONFIG:Release>:${_release}>")
target_link_options(hanvi_flags INTERFACE "$<$<CONFIG:Release>:${_release_link}>")

# ---- Compiler runtimes -------------------------------------------------------
# llvm-mingw runtime DLLs that have to ship next to the executables.
set(COMPILER_RUNTIMES "")
if (MINGW AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    foreach (_dll libc++.dll libunwind.dll libwinpthread-1.dll)
        find_file(_runtime_${_dll} ${_dll} HINTS "${_compiler_dir}" NO_DEFAULT_PATH)
        if (_runtime_${_dll})
            list(APPEND COMPILER_RUNTIMES "${_runtime_${_dll}}")
        endif ()
    endforeach ()
endif ()
