add_executable(Hanvi WIN32
        main.cpp
        resources.qrc
        app/app.cpp
        app/app.h
        app/desktop_io.cpp
        app/desktop_io.h
        components/mainwindow.cpp
        components/mainwindow.h
        components/mainwindow.ui
        components/dictpopup.cpp
        components/dictpopup.h
        components/dictpopup.ui
        components/namesetsmanager.cpp
        components/namesetsmanager.h
        components/namesetsmanager.ui
        components/rulepopup.cpp
        components/rulepopup.h
        components/rulepopup.ui
        components/namesetchooser.cpp
        components/namesetchooser.h
        components/namesetchooser.ui
        components/documents/document_session.cpp
        components/documents/document_session.h
        components/documents/token_canvas.cpp
        components/documents/token_canvas.h
        components/loader.h
        components/chipdelegate.h
        components/chiplist.h
        components/findbar.cpp
        components/findbar.h
        components/findbar.ui
        components/entrieseditor.cpp
        components/entrieseditor.h
        components/entrieseditor.ui
)

target_include_directories(Hanvi PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/components"
)

set_target_properties(Hanvi PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(Hanvi PRIVATE
        CoreLogic
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Sql
        Qt::Concurrent
        hanvi_flags
        $<$<PLATFORM_ID:Windows>:dwmapi>
)

if (WIN32)
    target_sources(Hanvi PRIVATE resources/icon.rc)
    # MSVC's linker embeds the manifest itself; MinGW needs it as a resource.
    if (MSVC)
        target_sources(Hanvi PRIVATE resources/app.manifest)
    else ()
        target_sources(Hanvi PRIVATE resources/manifest.rc)
    endif ()
endif ()

