# Copies the runtime data next to the executables so they run straight from the
# build directory. Expects HANVI_APPS to list the executable targets.
set(APP_DATA "${PROJECT_SOURCE_DIR}/dict.db" "${PROJECT_SOURCE_DIR}/desktop/resources/NotoSansSC.ttf")
set(_out "${CMAKE_BINARY_DIR}")
set(_data_stamp "${CMAKE_CURRENT_BINARY_DIR}/HanviData.stamp")

add_custom_command(
        OUTPUT "${_data_stamp}"
        DEPENDS ${OPENCC_DATA_FILES} ${APP_DATA}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_out}/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCC_DATA_FILES} "${_out}/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${APP_DATA} "${_out}"
        COMMAND ${CMAKE_COMMAND} -E touch "${_data_stamp}"
        VERBATIM)

add_custom_target(HanviRuntime ALL DEPENDS "${_data_stamp}")
if (TARGET Dictionaries)
    add_dependencies(HanviRuntime Dictionaries)
endif ()

if (WIN32)
    include(DeployWindows)
elseif (LINUX)
    include(DeployLinux)
endif ()
