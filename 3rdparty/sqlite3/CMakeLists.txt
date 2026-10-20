add_library(sqlite3 STATIC sqlite3.c)
set_target_properties(sqlite3 PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)
target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
else ()
    target_compile_options(sqlite3 PRIVATE -w)
endif ()

