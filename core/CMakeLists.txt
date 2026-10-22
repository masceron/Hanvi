include(FetchContent)

# Option to toggle building binary dictionaries (.ocd2) via host tools.
# When cross-compiling (e.g. for Android / iOS), this must be OFF because
# the cross-compiled opencc_dict tool cannot execute on the host machine.
option(HANVI_BUILD_OPENCC_DICTIONARIES "Generate OpenCC .ocd2 dictionary files with host tools" ON)
if (ANDROID OR CMAKE_CROSSCOMPILING)
    set(HANVI_BUILD_OPENCC_DICTIONARIES OFF CACHE BOOL "" FORCE)
endif ()

set(_opencc_patch_cmd "")
if (NOT HANVI_BUILD_OPENCC_DICTIONARIES)
    set(_opencc_patch_script "${CMAKE_CURRENT_BINARY_DIR}/patch_opencc_data.cmake")
    file(WRITE "${_opencc_patch_script}"
            "file(WRITE \"data/CMakeLists.txt\" \"add_custom_target(Dictionaries COMMENT \\\"OpenCC dictionaries disabled for cross-compilation\\\")\\n\")\n"
    )
    set(_opencc_patch_cmd PATCH_COMMAND ${CMAKE_COMMAND} -P "${_opencc_patch_script}")
endif ()

FetchContent_Declare(OpenCC
        GIT_REPOSITORY https://github.com/BYVoid/OpenCC.git
        GIT_TAG        ver.1.4.2
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
        ${_opencc_patch_cmd})

set(BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_GTEST OFF CACHE BOOL "" FORCE)
set(ENABLE_BENCHMARK OFF CACHE BOOL "" FORCE)
set(ENABLE_DARTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(OPENCC_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

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

# Expose OpenCC data paths to parent scope for packaging/deployment
set(OPENCC_CONFIG_FILE "${opencc_SOURCE_DIR}/data/config/t2s.json" CACHE INTERNAL "OpenCC config path")
set(OPENCC_DATA_FILES "${opencc_SOURCE_DIR}/data/config/t2s.json")
foreach (_dict TSPhrases TSCharacters TSCharactersExt CJK_Compatibility_Ideographs)
    list(APPEND OPENCC_DATA_FILES "${opencc_BINARY_DIR}/data/${_dict}.ocd2")
endforeach ()
set(OPENCC_DATA_FILES "${OPENCC_DATA_FILES}" PARENT_SCOPE)

# ---- SQLite ------------------------------------------------------------------
if (NOT TARGET sqlite3)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/sqlite3" "${CMAKE_CURRENT_BINARY_DIR}/sqlite3")
endif ()

# ---- CoreLogic ---------------------------------------------------------------
add_library(CoreLogic STATIC
        converter.h          converter.cpp
        db.h                 db.cpp
        dict.h               dict.cpp
        io.h                 io.cpp
        structures.h         structures.cpp
        aligned_document.h   aligned_document.cpp
        unicode_utils.h      sqlite_helper.h
)
set_target_properties(CoreLogic PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)

target_include_directories(CoreLogic PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_include_directories(CoreLogic SYSTEM PUBLIC
        "${opencc_SOURCE_DIR}/src"
        "${opencc_BINARY_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/sqlite3"
)
target_compile_definitions(CoreLogic PUBLIC Opencc_BUILT_AS_STATIC)
target_link_libraries(CoreLogic
        PUBLIC  sqlite3
        PRIVATE libopencc hanvi_flags
)

