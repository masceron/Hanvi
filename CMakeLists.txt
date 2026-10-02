cmake_minimum_required(VERSION 3.21)
project(Hanvi LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "Hanvi only supports Windows.")
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(HANVI_X86_64_V3 "Target x86-64-v3 (AVX2)" ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_COMPILE_OPTIONS_IPO   -flto=full)
    set(CMAKE_CXX_COMPILE_OPTIONS_IPO -flto=full)
endif ()

include(CheckIPOSupported)
check_ipo_supported(RESULT HANVI_IPO)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${HANVI_IPO})

# ---- Project-only flags -------------------------------
add_library(hanvi_flags INTERFACE)
if (MSVC)
    set(_warn         /W3 /WX /permissive-)
    set(_release      /Ob3 /Oi /Ot /GF /GA /Gy /Gw /Qpar)
    set(_release_link /OPT:REF /OPT:ICF)
    set(_arch         /arch:AVX2)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(_warn         -Wall -Wextra -Werror -pedantic-errors)
    set(_release      -fomit-frame-pointer -ffunction-sections -fdata-sections)
    set(_release_link -s -Wl,--gc-sections)
    set(_arch         -march=x86-64-v3 -mtune=generic)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND _release -fipa-pta)
        list(APPEND _release_link -flto-partition=one)
    else ()
        list(APPEND _release_link -Wl,--icf=all)
    endif ()
else ()
    message(FATAL_ERROR "Compiler not supported.")
endif ()

if (MSVC)
    target_compile_definitions(hanvi_flags INTERFACE NOMINMAX)
endif ()

target_compile_options(hanvi_flags INTERFACE ${_warn}
        "$<$<CONFIG:Release>:${_release}>"
        "$<$<BOOL:${HANVI_X86_64_V3}>:${_arch}>")
target_link_options(hanvi_flags INTERFACE "$<$<CONFIG:Release>:${_release_link}>")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Sql Widgets Concurrent)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6Core_DIR}/../../../bin" "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}" REQUIRED)

set(COMPILER_RUNTIMES "")
cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _compiler_bin)
foreach (_dll libc++ libunwind libwinpthread-1 libstdc++-6 libgcc_s_seh-1)
    if (EXISTS "${_compiler_bin}/${_dll}.dll")
        list(APPEND COMPILER_RUNTIMES "${_compiler_bin}/${_dll}.dll")
    endif ()
endforeach ()

include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_DOCUMENTATION OFF)
set(BUILD_PYTHON OFF)
set(ENABLE_BENCHMARK OFF)
set(ENABLE_COVERAGE OFF)
set(ENABLE_GTEST OFF)
set(BUILD_SHARED_LIBS OFF)
set(OPENCC_ENABLE_INSTALL OFF)
FetchContent_Declare(OpenCC
        GIT_REPOSITORY https://github.com/BYVoid/OpenCC.git
        GIT_TAG        ver.1.4.2
        GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(OpenCC)

if (COMPILER_RUNTIMES AND TARGET Dictionaries)
    set(_opencc_tools_stamp "${opencc_BINARY_DIR}/src/tools/runtimes.stamp")
    add_custom_command(
            OUTPUT "${_opencc_tools_stamp}"
            DEPENDS ${COMPILER_RUNTIMES}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${opencc_BINARY_DIR}/src/tools"
            COMMAND ${CMAKE_COMMAND} -E touch "${_opencc_tools_stamp}"
            VERBATIM)
    add_custom_target(copy_compiler_runtimes_to_opencc DEPENDS "${_opencc_tools_stamp}")
    add_dependencies(Dictionaries copy_compiler_runtimes_to_opencc)
endif ()

set(OPENCC_DATA "${opencc_SOURCE_DIR}/data/config/t2s.json")
foreach (_dict TSPhrases TSCharacters TSCharactersExt CJK_Compatibility_Ideographs)
    list(APPEND OPENCC_DATA "${opencc_BINARY_DIR}/data/${_dict}.ocd2")
endforeach ()
set(APP_DATA "${CMAKE_CURRENT_SOURCE_DIR}/dict.db" "${CMAKE_CURRENT_SOURCE_DIR}/resources/NotoSansSC.ttf")

# ---- Targets -----------------------------------------------------------------
add_library(CoreLogic STATIC
        core/converter.h   core/converter.cpp
        core/db.h          core/db.cpp
        core/dict.h        core/dict.cpp
        core/io.h          core/io.cpp
        core/structures.h  core/structures.cpp
        components/documents/aligned_document.h
        components/documents/aligned_document.cpp)
target_include_directories(CoreLogic PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/components")
target_include_directories(CoreLogic SYSTEM PUBLIC
        "${opencc_SOURCE_DIR}/src" "${opencc_BINARY_DIR}/src")
target_compile_definitions(CoreLogic PUBLIC Opencc_BUILT_AS_STATIC)
target_link_libraries(CoreLogic
        PUBLIC  Qt::Core Qt::Sql Qt::Concurrent
        PRIVATE Qt::Widgets libopencc hanvi_flags)

add_executable(Hanvi WIN32
        main.cpp resources.qrc resources/icon.rc
        app/app.cpp app/app.h
        components/mainwindow.cpp        components/mainwindow.h        components/mainwindow.ui
        components/dictpopup.cpp         components/dictpopup.h         components/dictpopup.ui
        components/namesetsmanager.cpp   components/namesetsmanager.h   components/namesetsmanager.ui
        components/rulepopup.cpp         components/rulepopup.h         components/rulepopup.ui
        components/namesetchooser.cpp    components/namesetchooser.h    components/namesetchooser.ui
        components/documents/document_session.cpp components/documents/document_session.h
        components/documents/token_canvas.cpp     components/documents/token_canvas.h
        components/loader.h components/chipdelegate.h components/chiplist.h
        components/findbar.cpp
        components/findbar.h
        components/findbar.ui)
target_link_libraries(Hanvi PRIVATE CoreLogic Qt::Gui Qt::Widgets dwmapi hanvi_flags)
if (MSVC)
    target_sources(Hanvi PRIVATE resources/app.manifest)
else ()
    target_sources(Hanvi PRIVATE resources/manifest.rc)
endif ()

add_executable(HanviCLI main_cli.cpp)
target_link_libraries(HanviCLI PRIVATE CoreLogic hanvi_flags)

set(_out "$<TARGET_FILE_DIR:Hanvi>")
set(_deploy_stamp "${CMAKE_CURRENT_BINARY_DIR}/HanviRuntime-$<CONFIG>.stamp")
set(_copy_runtimes "")
if (COMPILER_RUNTIMES)
    set(_copy_runtimes COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${_out}")
endif ()

add_custom_command(
        OUTPUT "${_deploy_stamp}"
        DEPENDS Hanvi HanviCLI ${OPENCC_DATA} ${APP_DATA} ${COMPILER_RUNTIMES}
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-compiler-runtime
                "$<TARGET_FILE:Hanvi>" "$<TARGET_FILE:HanviCLI>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_out}/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCC_DATA} "${_out}/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${APP_DATA} "${_out}"
        ${_copy_runtimes}
        COMMAND ${CMAKE_COMMAND} -E touch "${_deploy_stamp}"
        VERBATIM)

add_custom_target(HanviRuntime ALL DEPENDS "${_deploy_stamp}")
if (TARGET Dictionaries)
    add_dependencies(HanviRuntime Dictionaries)
endif ()

find_program(SEVENZIP_EXECUTABLE NAMES 7z 7za)
if (SEVENZIP_EXECUTABLE)
    set(DIST_DIR "${CMAKE_BINARY_DIR}/Hanvi")
    set(ARCHIVE  "${CMAKE_BINARY_DIR}/Hanvi.7z")

    if (COMPILER_RUNTIMES)
        set(_zip_runtimes COMMAND ${CMAKE_COMMAND} -E copy ${COMPILER_RUNTIMES} "${DIST_DIR}")
        set(_crt --no-compiler-runtime)
    else ()
        set(_zip_runtimes "")
        set(_crt --compiler-runtime)
    endif ()

    add_custom_target(MakePortableZip
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}" "${ARCHIVE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/opencc"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Hanvi>" "$<TARGET_FILE:HanviCLI>"
            ${APP_DATA} "${DIST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy ${OPENCC_DATA} "${DIST_DIR}/opencc"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations ${_crt}
            "${DIST_DIR}/$<TARGET_FILE_NAME:Hanvi>" "${DIST_DIR}/$<TARGET_FILE_NAME:HanviCLI>"
            ${_zip_runtimes}
            COMMAND "${SEVENZIP_EXECUTABLE}" a -t7z -mx=9 "${ARCHIVE}" "${DIST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}"
            VERBATIM)

    add_dependencies(MakePortableZip HanviRuntime)
else ()
    message(STATUS "7z not found; MakePortableZip target disabled.")
endif ()

