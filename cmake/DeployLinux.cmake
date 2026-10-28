# Portable archive with the same flat layout as on Windows; Qt goes into lib/
# and plugins/. Staged with `cmake --install`, which runs Qt's deploy script.
#
# Qt's deploy tool resolves libraries through the executable's RUNPATH only, so
# the installed apps keep the Qt lib dir until deployment has finished and the
# script below resets it to just $ORIGIN/lib.
set_target_properties(${HANVI_APPS} PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib;${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBS}")

install(TARGETS ${HANVI_APPS} RUNTIME DESTINATION .)
install(FILES ${APP_DATA} DESTINATION .)
install(FILES ${OPENCC_DATA_FILES} DESTINATION opencc)

set(_app_files ${HANVI_APPS})
list(TRANSFORM _app_files REPLACE "(.+)" "\"\${QT_DEPLOY_PREFIX}/$<TARGET_FILE_NAME:\\1>\"")
list(JOIN _app_files " " _app_files)
list(GET HANVI_APPS 0 _main_app)

qt_policy(SET QTP0008 NEW)
qt_generate_deploy_script(
        TARGET ${_main_app}
        OUTPUT_SCRIPT _qt_deploy_script
        CONTENT "
set(QT_DEPLOY_BIN_DIR .)
set(QT_DEPLOY_LIB_DIR lib)
set(QT_DEPLOY_PLUGINS_DIR plugins)
set(apps ${_app_files})
set(extra_apps \${apps})
list(POP_FRONT extra_apps main_app)
qt_deploy_runtime_dependencies(
    EXECUTABLE \"\${main_app}\"
    ADDITIONAL_EXECUTABLES \${extra_apps}
    INCLUDE_PLUGINS qwayland
    EXCLUDE_PLUGIN_TYPES egldeviceintegrations generic networkinformation tls
    GENERATE_QT_CONF
    NO_TRANSLATIONS
)
foreach (app IN LISTS apps)
    file(RPATH_SET FILE \"\${app}\" NEW_RPATH \"$ORIGIN/lib\")
endforeach ()")
install(SCRIPT "${_qt_deploy_script}")

# ---- Portable archive --------------------------------------------------------
# Prefer 7-Zip's own Linux binary; `7z` may be the older p7zip fork.
find_program(SEVENZIP_EXECUTABLE NAMES 7zz 7z 7za)
if (NOT SEVENZIP_EXECUTABLE)
    return()
endif ()

set(DIST_DIR "${CMAKE_BINARY_DIR}/dist/Hanvi")
set(ARCHIVE  "${CMAKE_BINARY_DIR}/Hanvi.7z")

# -snl stores the Qt library symlinks as links instead of duplicating the files.
add_custom_target(MakePortableZip
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}" "${ARCHIVE}"
        COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${DIST_DIR}" --config $<CONFIG>
        COMMAND "${SEVENZIP_EXECUTABLE}" a -t7z -mx=9 -snl "${ARCHIVE}" "${DIST_DIR}"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${DIST_DIR}"
        VERBATIM)

add_dependencies(MakePortableZip HanviRuntime)
