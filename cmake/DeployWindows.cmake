# Qt and compiler runtimes for the build directory, plus the portable archive.
find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6Core_DIR}/../../../bin" "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}" REQUIRED)

set(_app_files ${HANVI_APPS})
list(TRANSFORM _app_files REPLACE "(.+)" "$<TARGET_FILE:\\1>")

# ---- Build directory ---------------------------------------------------------
set(_copy_runtimes "")
if (COMPILER_RUNTIMES)
    set(_copy_runtimes COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${_out}")
endif ()

set(_qt_stamp "${CMAKE_CURRENT_BINARY_DIR}/HanviQt-$<CONFIG>.stamp")
add_custom_command(
        OUTPUT "${_qt_stamp}"
        DEPENDS ${HANVI_APPS} ${COMPILER_RUNTIMES}
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-compiler-runtime ${_app_files}
        ${_copy_runtimes}
        COMMAND ${CMAKE_COMMAND} -E touch "${_qt_stamp}"
        VERBATIM)

add_custom_target(HanviQtRuntime ALL DEPENDS "${_qt_stamp}")
add_dependencies(HanviRuntime HanviQtRuntime)

# ---- Portable archive --------------------------------------------------------
find_program(SEVENZIP_EXECUTABLE NAMES 7z 7za)
if (NOT SEVENZIP_EXECUTABLE)
    return()
endif ()

set(DIST_DIR "${CMAKE_BINARY_DIR}/Hanvi")
set(ARCHIVE  "${CMAKE_BINARY_DIR}/Hanvi.7z")

set(_dist_files ${HANVI_APPS})
list(TRANSFORM _dist_files REPLACE "(.+)" "${DIST_DIR}/$<TARGET_FILE_NAME:\\1>")

# Ship the llvm-mingw runtimes if we have them, otherwise let windeployqt add one.
if (COMPILER_RUNTIMES)
    set(_crt --no-compiler-runtime)
    set(_zip_runtimes COMMAND ${CMAKE_COMMAND} -E copy_if_different ${COMPILER_RUNTIMES} "${DIST_DIR}")
else ()
    set(_crt --compiler-runtime)
    set(_zip_runtimes "")
endif ()

add_custom_target(MakePortableZip
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}" "${ARCHIVE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy ${_app_files} ${APP_DATA} "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy ${OPENCC_DATA_FILES} "${DIST_DIR}/opencc"
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations ${_crt} ${_dist_files}
        ${_zip_runtimes}
        COMMAND "${SEVENZIP_EXECUTABLE}" a -t7z -mx=9 "${ARCHIVE}" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}"
        VERBATIM)

add_dependencies(MakePortableZip HanviRuntime)
