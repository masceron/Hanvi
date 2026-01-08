cmake_minimum_required(VERSION 4.1)
project(Converter)

set(CMAKE_CXX_STANDARD 26)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_PREFIX_PATH "D:/Programs/Qt/6.10.1/msvc2022_64")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

        string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

        add_compile_options(/O2 /Oi /Ot /Ob3 /GL /GF /GA /Gy /Gw /Qpar /EHs-c- /D_HAS_EXCEPTIONS=0 /arch:AVX512)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_PREFIX_PATH "D:/Programs/Qt/6.10.1/mingw_64")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3 -fno-exceptions -ffunction-sections -fdata-sections -march=native -mtune=native -DNDEBUG)
        add_link_options(-s)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_PREFIX_PATH "D:/Programs/Qt/6.10.1/llvm-mingw_64")
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3 -fno-exceptions -flto=full -ffunction-sections -fdata-sections -march=native -mtune=native -DNDEBUG)
        add_link_options(-flto=full -Wl,--gc-sections -Wl,--icf=all)
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
        core/trie.h
        core/trie.cpp
)

add_library(CoreLogic STATIC ${CORE})
target_link_libraries(CoreLogic PRIVATE Qt::Core Qt::Widgets Qt::Sql Qt::Concurrent)

add_executable(Converter WIN32 main.cpp
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
)

target_include_directories(Converter PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/components"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Converter PRIVATE CoreLogic Qt::Core Qt::Sql Qt::Gui Qt::Widgets Qt::Concurrent)

add_executable(ConverterCLI main_cli.cpp)
target_link_libraries(ConverterCLI PRIVATE CoreLogic Qt::Core Qt::Sql Qt::Concurrent)

if (WIN32)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin")

    if (WINDEPLOYQT_EXE)
        add_custom_command(TARGET Converter POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                --verbose 0
                --no-translations
                --no-compiler-runtime
                --dir "$<TARGET_FILE_DIR:Converter>"
                "$<TARGET_FILE:Converter>"
        )

        add_custom_command(TARGET Converter POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/dict.db"
                "$<TARGET_FILE_DIR:Converter>/dict.db"
        )

        add_custom_command(TARGET ConverterCLI POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                --verbose 0
                --no-translations
                --no-compiler-runtime
                --dir "$<TARGET_FILE_DIR:ConverterCLI>"
                "$<TARGET_FILE:ConverterCLI>"
        )
    endif ()
endif ()

if (WIN32 AND WINDEPLOYQT_EXE)
    set(DIST_DIR "${CMAKE_BINARY_DIR}/Converter")
    set(ZIP_NAME "Converter.zip")

    add_custom_target(MakePortableZip ALL
            DEPENDS Converter ConverterCLI
    )

    add_custom_command(TARGET MakePortableZip POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E remove_directory "${DIST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"

            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Converter>" "${DIST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ConverterCLI>" "${DIST_DIR}"

            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/dict.db" "${DIST_DIR}"

            COMMAND "${WINDEPLOYQT_EXE}"
            --dir "${DIST_DIR}"
            --no-translations
            --no-compiler-runtime
            "${DIST_DIR}/$<TARGET_FILE_NAME:Converter>"

            COMMAND "${WINDEPLOYQT_EXE}"
            --dir "${DIST_DIR}"
            --no-translations
            --no-compiler-runtime
            "${DIST_DIR}/$<TARGET_FILE_NAME:ConverterCLI>"

            COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/${ZIP_NAME}" --format=zip "${DIST_DIR}"
    )
endif ()