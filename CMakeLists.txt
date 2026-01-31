cmake_minimum_required(VERSION 3.20)
project(Hanvi)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(BUILD_PORTABLE "Build for distribution" ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
        string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

        add_compile_options(/O2 /Oi /Ot /Ob3 /GL /GF /GA /Gy /Gw /Qpar /EHs-c- /D_HAS_EXCEPTIONS=0 /W3 /WX /permissive-)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3 -fomit-frame-pointer -fno-exceptions -ffunction-sections -fdata-sections -Wall -Wextra -Werror -pedantic-errors -DNDEBUG)
        add_link_options(-s)

        if (BUILD_PORTABLE)
            add_compile_options(-mtune=generic -march=x86-64-v2)
        else()
            add_compile_options(-march=native -mtune=native)
        endif()
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3 -fomit-frame-pointer -fno-exceptions -flto=full -ffunction-sections -fdata-sections -Wall -Wextra -Werror -pedantic-errors -DNDEBUG)
        add_link_options(-flto=full -Wl,--gc-sections -Wl,--icf=all)

        if (BUILD_PORTABLE)
            add_compile_options(-mtune=generic -march=x86-64-v2)
        else()
            add_compile_options(-march=native -mtune=native)
        endif()
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
)

add_library(CoreLogic STATIC ${CORE})
target_link_libraries(CoreLogic PRIVATE Qt::Core Qt::Widgets Qt::Sql Qt::Concurrent)

add_executable(Hanvi WIN32 main.cpp
        components/mainwindow.cpp
        components/mainwindow.h
        components/mainwindow.ui
        app/app.cpp
        app/app.h
        components/dictpopup.cpp
        components/dictpopup.h
        components/dictpopup.ui
        components/loader.h
        resources.qrc
        components/chipdelegate.h
        components/chiplist.h
        resources/icon.rc
        components/namesetsmanager.cpp
        components/namesetsmanager.h
        components/namesetsmanager.ui
        components/rulepopup.cpp
        components/rulepopup.h
        components/rulepopup.ui
)

target_include_directories(Hanvi PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/components"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Hanvi PRIVATE CoreLogic Qt::Core Qt::Sql Qt::Gui Qt::Widgets Qt::Concurrent)

add_executable(HanviCLI main_cli.cpp)
target_link_libraries(HanviCLI PRIVATE CoreLogic Qt::Core Qt::Sql Qt::Concurrent)

if (WIN32)
    if(NOT WINDEPLOYQT_EXECUTABLE)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/bin")
    endif()

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET Hanvi POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --verbose 0
                --no-translations
                --no-compiler-runtime
                --dir "$<TARGET_FILE_DIR:Hanvi>"
                "$<TARGET_FILE:Hanvi>"
        )

        add_custom_command(TARGET Hanvi POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/dict.db"
                "$<TARGET_FILE_DIR:Hanvi>/dict.db"
        )

        add_custom_command(TARGET HanviCLI POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --verbose 0
                --no-translations
                --no-compiler-runtime
                --dir "$<TARGET_FILE_DIR:HanviCLI>"
                "$<TARGET_FILE:HanviCLI>"
        )

        set(DIST_DIR "${CMAKE_BINARY_DIR}/Hanvi")
        set(ZIP_NAME "Hanvi.zip")

        add_custom_target(MakePortableZip ALL
                DEPENDS Hanvi HanviCLI
        )

        add_custom_command(TARGET MakePortableZip POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E remove_directory "${DIST_DIR}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"

                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Hanvi>" "${DIST_DIR}"
                COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:HanviCLI>" "${DIST_DIR}"

                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/resources/NotoSansSC.ttf" ${DIST_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/dict.db" "${DIST_DIR}"

                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --dir "${DIST_DIR}"
                --no-translations
                --compiler-runtime
                "${DIST_DIR}/$<TARGET_FILE_NAME:Hanvi>"

                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --dir "${DIST_DIR}"
                --no-translations
                --compiler-runtime
                "${DIST_DIR}/$<TARGET_FILE_NAME:HanviCLI>"

                COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/${ZIP_NAME}" --format=zip "${DIST_DIR}"
                COMMAND ${CMAKE_COMMAND} -E rm -r "${DIST_DIR}"
        )
    else ()
        message(FATAL_ERROR "Windeployqt not found.")
    endif ()
endif ()