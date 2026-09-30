cmake_minimum_required(VERSION 3.20)
project(Hanvi)

if (NOT WIN32)
    message(FATAL_ERROR "Hanvi only supports Windows.")
endif ()

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

        add_compile_options(/O2 /Oi /Ot /Ob3
                /GL /GF /GA /Gy /Gw
                /Qpar /EHs-c- /D_HAS_EXCEPTIONS=0
                /W3 /WX /permissive- /arch:AVX2)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3 -fomit-frame-pointer -fno-exceptions
                -flto=auto -fipa-pta -flto-partition=one -fwhole-program
                -ffunction-sections -fdata-sections
                -Wall -Wextra -Werror -pedantic-errors -DNDEBUG -mtune=generic -march=x86-64-v3)
        add_link_options(-flto=auto -fwhole-program -s)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3 -fomit-frame-pointer
                -flto=full -ffunction-sections
                -fdata-sections -Wall -Wextra
                -DNDEBUG -mtune=generic -march=x86-64-v3)
        add_link_options(-flto=full -s -Wl,--gc-sections -Wl,--icf=all)
    endif ()
else ()
    message(FATAL_ERROR "Compiler not supported.")
endif ()

find_package(Qt6 COMPONENTS
        Core
        Gui
        Sql
        Widgets
        Concurrent
        REQUIRED)

if (WIN32)
    get_filename_component(COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(COMPILER_RUNTIMES "")
    foreach(_dll libc++.dll libunwind.dll libwinpthread-1.dll libstdc++-6.dll libgcc_s_seh-1.dll)
        if (EXISTS "${COMPILER_BIN_DIR}/${_dll}")
            list(APPEND COMPILER_RUNTIMES "${COMPILER_BIN_DIR}/${_dll}")
        endif ()
    endforeach()
endif ()

include(FetchContent)
set(BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(ENABLE_BENCHMARK OFF CACHE BOOL "" FORCE)
set(ENABLE_COVERAGE OFF CACHE BOOL "" FORCE)
set(ENABLE_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(OPENCC_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    OpenCC
    GIT_REPOSITORY https://github.com/BYVoid/OpenCC.git
    GIT_TAG        ver.1.4.2
)
FetchContent_MakeAvailable(OpenCC)

if (WIN32 AND COMPILER_RUNTIMES)
    add_custom_target(copy_compiler_runtimes_to_opencc
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${opencc_BINARY_DIR}/src/tools"
    )
    if (TARGET Dictionaries)
        add_dependencies(Dictionaries copy_compiler_runtimes_to_opencc)
    endif ()
endif ()

set(CORE
        core/converter.h
        core/converter.cpp
        core/db.h
        core/db.cpp
        core/dict.h
        core/dict.cpp
        core/io.h
        core/io.cpp
        core/structures.h
        core/structures.cpp
        components/documents/aligned_document.h
        components/documents/aligned_document.cpp
)

add_library(CoreLogic STATIC ${CORE})
target_include_directories(CoreLogic PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/components"
        "${opencc_SOURCE_DIR}/src"
        "${opencc_BINARY_DIR}/src"
)
target_link_libraries(CoreLogic PRIVATE Qt::Core Qt::Widgets Qt::Sql Qt::Concurrent libopencc)
target_compile_definitions(CoreLogic PUBLIC Opencc_BUILT_AS_STATIC)

set(APP_SOURCES main.cpp
        components/mainwindow.cpp
        components/mainwindow.h
        components/mainwindow.ui
        app/app.cpp
        app/app.h
        components/documents/document_session.h
        components/documents/document_session.cpp
        components/documents/token_canvas.h
        components/documents/token_canvas.cpp
        components/dictpopup.cpp
        components/dictpopup.h
        components/dictpopup.ui
        components/loader.h
        resources.qrc
        components/chipdelegate.h
        components/chiplist.h
        components/namesetsmanager.cpp
        components/namesetsmanager.h
        components/namesetsmanager.ui
        components/rulepopup.cpp
        components/rulepopup.h
        components/rulepopup.ui
        components/namesetchooser.cpp
        components/namesetchooser.h
        components/namesetchooser.ui
)

add_executable(Hanvi WIN32 ${APP_SOURCES} resources/icon.rc)

target_include_directories(Hanvi PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/components"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(Hanvi PRIVATE CoreLogic Qt::Core Qt::Sql Qt::Gui Qt::Widgets Qt::Concurrent dwmapi)

add_executable(HanviCLI main_cli.cpp)
target_link_libraries(HanviCLI PRIVATE CoreLogic Qt::Core Qt::Sql Qt::Concurrent)

if (TARGET Dictionaries)
    add_dependencies(Hanvi Dictionaries)
    add_dependencies(HanviCLI Dictionaries)
endif ()

add_custom_command(TARGET Hanvi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/dict.db"
        "$<TARGET_FILE_DIR:Hanvi>/dict.db"
)
add_custom_command(TARGET Hanvi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/NotoSansSC.ttf"
        "$<TARGET_FILE_DIR:Hanvi>/NotoSansSC.ttf"
)
add_custom_command(TARGET Hanvi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Hanvi>/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_SOURCE_DIR}/data/config/t2s.json"
        "$<TARGET_FILE_DIR:Hanvi>/opencc/t2s.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/TSPhrases.ocd2"
        "$<TARGET_FILE_DIR:Hanvi>/opencc/TSPhrases.ocd2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/TSCharacters.ocd2"
        "$<TARGET_FILE_DIR:Hanvi>/opencc/TSCharacters.ocd2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/TSCharactersExt.ocd2"
        "$<TARGET_FILE_DIR:Hanvi>/opencc/TSCharactersExt.ocd2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/CJK_Compatibility_Ideographs.ocd2"
        "$<TARGET_FILE_DIR:Hanvi>/opencc/CJK_Compatibility_Ideographs.ocd2"
)

add_custom_command(TARGET HanviCLI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:HanviCLI>/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_SOURCE_DIR}/data/config/t2s.json"
        "$<TARGET_FILE_DIR:HanviCLI>/opencc/t2s.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/TSPhrases.ocd2"
        "$<TARGET_FILE_DIR:HanviCLI>/opencc/TSPhrases.ocd2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/TSCharacters.ocd2"
        "$<TARGET_FILE_DIR:HanviCLI>/opencc/TSCharacters.ocd2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/TSCharactersExt.ocd2"
        "$<TARGET_FILE_DIR:HanviCLI>/opencc/TSCharactersExt.ocd2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${opencc_BINARY_DIR}/data/CJK_Compatibility_Ideographs.ocd2"
        "$<TARGET_FILE_DIR:HanviCLI>/opencc/CJK_Compatibility_Ideographs.ocd2"
)

if (NOT WINDEPLOYQT_EXECUTABLE)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/bin")
endif ()

if (NOT WINDEPLOYQT_EXECUTABLE)
    message(FATAL_ERROR "Windeployqt not found.")
endif ()


add_custom_command(TARGET Hanvi POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --dir "${CMAKE_BINARY_DIR}" --no-translations --no-compiler-runtime "$<TARGET_FILE_NAME:Hanvi>"
)
add_custom_command(TARGET HanviCLI POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --dir "${CMAKE_BINARY_DIR}" --no-translations --no-compiler-runtime "$<TARGET_FILE_NAME:HanviCLI>"
)

if (COMPILER_RUNTIMES)
    add_custom_command(TARGET Hanvi POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${CMAKE_BINARY_DIR}"
    )
    add_custom_command(TARGET HanviCLI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${CMAKE_BINARY_DIR}"
    )
    set(WINDEPLOYQT_COMPILER_RUNTIME_FLAG "--no-compiler-runtime")
else ()
    set(WINDEPLOYQT_COMPILER_RUNTIME_FLAG "--compiler-runtime")
endif ()

set(DIST_DIR "${CMAKE_BINARY_DIR}/Hanvi")

set(ARCHIVE_NAME "Hanvi.7z")
add_custom_target(MakePortableZip
        DEPENDS Hanvi HanviCLI
)

add_custom_command(TARGET MakePortableZip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Hanvi>" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:HanviCLI>" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/NotoSansSC.ttf" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/dict.db" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:Hanvi>/opencc" "${DIST_DIR}/opencc"
)

add_custom_command(TARGET MakePortableZip POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --dir "${DIST_DIR}" --no-translations ${WINDEPLOYQT_COMPILER_RUNTIME_FLAG} "${DIST_DIR}/$<TARGET_FILE_NAME:Hanvi>"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --dir "${DIST_DIR}" --no-translations ${WINDEPLOYQT_COMPILER_RUNTIME_FLAG} "${DIST_DIR}/$<TARGET_FILE_NAME:HanviCLI>"
)

if (COMPILER_RUNTIMES)
    add_custom_command(TARGET MakePortableZip POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${DIST_DIR}"
    )
endif ()

add_custom_command(TARGET MakePortableZip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}"
        COMMAND 7z a -t7z -mx=9 "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -r "${DIST_DIR}"
)

