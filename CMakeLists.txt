cmake_minimum_required(VERSION 3.21)
project(Hanvi LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(HANVI_IPO "Compile with IPO" ON)
option(HANVI_BUILD_DESKTOP "Build Desktop Qt application" ON)
option(HANVI_BUILD_CLI "Build CLI conversion tool" ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if (NOT ipo_supported)
    message(WARNING "IPO is not supported: ${ipo_error}")
    set(HANVI_IPO OFF)
endif ()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${HANVI_IPO})

# ---- Project-only flags -------------------------------
add_library(hanvi_flags INTERFACE)
if (MSVC)
    set(_warn         /W3 /WX /permissive- /utf-8)
    set(_release      /Ob3 /Oi /Ot /GF /GA /Gy /Gw /Qpar)
    set(_release_link /OPT:REF /OPT:ICF)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(_warn         -Wall -Wextra -Werror -pedantic-errors)
    set(_release      -fomit-frame-pointer -ffunction-sections -fdata-sections)
    set(_release_link -s -Wl,--gc-sections)
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
        "$<$<CONFIG:Release>:${_release}>")
target_link_options(hanvi_flags INTERFACE "$<$<CONFIG:Release>:${_release_link}>")

if (HANVI_BUILD_DESKTOP OR HANVI_BUILD_CLI)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Sql Widgets Concurrent)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
            HINTS "${Qt6Core_DIR}/../../../bin" "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}" REQUIRED)
endif ()

set(COMPILER_RUNTIMES "")
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(_libcxx "libc++.dll" HINTS "${_compiler_dir}" NO_DEFAULT_PATH)
    find_file(_libunwind "libunwind.dll" HINTS "${_compiler_dir}" NO_DEFAULT_PATH)
    find_file(_libwinpthread "libwinpthread-1.dll" HINTS "${_compiler_dir}" NO_DEFAULT_PATH)
    if (_libcxx)
        list(APPEND COMPILER_RUNTIMES "${_libcxx}")
    endif ()
    if (_libunwind)
        list(APPEND COMPILER_RUNTIMES "${_libunwind}")
    endif ()
    if (_libwinpthread)
        list(APPEND COMPILER_RUNTIMES "${_libwinpthread}")
    endif ()
endif ()

# ---- Core Logic -------------------------------------------------------------
add_subdirectory(core)

# ---- Ports / Frontends ------------------------------------------------------
if (HANVI_BUILD_DESKTOP)
    add_subdirectory(desktop)
endif ()

if (HANVI_BUILD_CLI)
    add_subdirectory(cli)
endif ()

# ---- Deployment (Windows) ----------------------------------------------------
if (WIN32 AND (HANVI_BUILD_DESKTOP OR HANVI_BUILD_CLI))
    set(APP_DATA "${CMAKE_CURRENT_SOURCE_DIR}/dict.db" "${CMAKE_CURRENT_SOURCE_DIR}/desktop/resources/NotoSansSC.ttf")
    set(_out "${CMAKE_BINARY_DIR}")
    set(_deploy_stamp "${CMAKE_CURRENT_BINARY_DIR}/HanviRuntime-$<CONFIG>.stamp")
    set(_copy_runtimes "")
    if (COMPILER_RUNTIMES)
        set(_copy_runtimes COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${_out}")
    endif ()

    set(_deploy_targets "")
    if (TARGET Hanvi)
        list(APPEND _deploy_targets "$<TARGET_FILE:Hanvi>")
    endif ()
    if (TARGET HanviCLI)
        list(APPEND _deploy_targets "$<TARGET_FILE:HanviCLI>")
    endif ()

    add_custom_command(
            OUTPUT "${_deploy_stamp}"
            DEPENDS ${_deploy_targets} ${OPENCC_DATA_FILES} ${APP_DATA} ${COMPILER_RUNTIMES}
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-compiler-runtime
                    ${_deploy_targets}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_out}/opencc"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCC_DATA_FILES} "${_out}/opencc"
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
            set(_zip_runtimes COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${DIST_DIR}")
            set(_crt --no-compiler-runtime)
        else ()
            set(_zip_runtimes "")
            set(_crt --compiler-runtime)
        endif ()

        set(_zip_copy_targets "")
        set(_zip_deploy_args "")
        if (TARGET Hanvi)
            list(APPEND _zip_copy_targets "$<TARGET_FILE:Hanvi>")
            list(APPEND _zip_deploy_args "${DIST_DIR}/$<TARGET_FILE_NAME:Hanvi>")
        endif ()
        if (TARGET HanviCLI)
            list(APPEND _zip_copy_targets "$<TARGET_FILE:HanviCLI>")
            list(APPEND _zip_deploy_args "${DIST_DIR}/$<TARGET_FILE_NAME:HanviCLI>")
        endif ()

        add_custom_target(MakePortableZip
                COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}" "${ARCHIVE}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/opencc"
                COMMAND ${CMAKE_COMMAND} -E copy ${_zip_copy_targets} ${APP_DATA} "${DIST_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy ${OPENCC_DATA_FILES} "${DIST_DIR}/opencc"
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations ${_crt}
                        ${_zip_deploy_args}
                ${_zip_runtimes}
                COMMAND "${SEVENZIP_EXECUTABLE}" a -t7z -mx=9 "${ARCHIVE}" "${DIST_DIR}"
                COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}"
                VERBATIM)

        add_dependencies(MakePortableZip HanviRuntime)
    endif ()
endif ()
