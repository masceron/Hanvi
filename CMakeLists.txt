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
        add_compile_options(-O3 -fomit-frame-pointer -fno-exceptions
                -flto=full -ffunction-sections
                -fdata-sections -Wall -Wextra -Werror
                -pedantic-errors -DNDEBUG -mtune=generic -march=x86-64-v3)
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
)
target_link_libraries(CoreLogic PRIVATE Qt::Core Qt::Widgets Qt::Sql Qt::Concurrent)

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

add_custom_command(TARGET Hanvi POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/dict.db"
        "$<TARGET_FILE_DIR:Hanvi>/dict.db"
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

set(DIST_DIR "${CMAKE_BINARY_DIR}/Hanvi")

set(ARCHIVE_NAME "Hanvi.7z")
add_custom_target(MakePortableZip ALL
        DEPENDS Hanvi HanviCLI
)

add_custom_command(TARGET MakePortableZip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Hanvi>" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:HanviCLI>" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/NotoSansSC.ttf" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/dict.db" "${DIST_DIR}"
)

add_custom_command(TARGET MakePortableZip POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --dir "${DIST_DIR}" --no-translations --compiler-runtime "${DIST_DIR}/$<TARGET_FILE_NAME:Hanvi>"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --dir "${DIST_DIR}" --no-translations --compiler-runtime "${DIST_DIR}/$<TARGET_FILE_NAME:HanviCLI>"
)

add_custom_command(TARGET MakePortableZip POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}"
        COMMAND 7z a -t7z -mx=9 "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -r "${DIST_DIR}"
)
