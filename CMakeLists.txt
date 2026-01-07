cmake_minimum_required(VERSION 4.1)
project(Converter)

set(CMAKE_CXX_STANDARD 26)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/Programs/Qt/6.10.1/msvc2022_64")

if(CMAKE_BUILD_TYPE MATCHES "Release")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/Ob2" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

    add_compile_options(/O2 /Oi /Ot /Ob3 /GL /GF /GA /Gy /Gw /Qpar /fp:fast /EHs-c- /D_HAS_EXCEPTIONS=0 /arch:AVX512)
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
endif()

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
)
target_link_libraries(Converter PRIVATE CoreLogic Qt::Core Qt::Sql Qt::Gui Qt::Widgets Qt::Concurrent)

add_executable(ConverterCLI main_cli.cpp)
target_link_libraries(ConverterCLI PRIVATE CoreLogic Qt::Core Qt::Sql)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/sqldrivers/qsqlite${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers/")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/sqldrivers/qsqlite${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sqldrivers/")
    endif ()

    if (EXISTS "${QT_INSTALL_PATH}/plugins/imageformats/qgif${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/imageformats/")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/imageformats/qgif${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/imageformats/")
    endif ()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/dict.db"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/dict.db")

    foreach (QT_LIB Core Gui Widgets Sql)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()
