# Embind module exposing the core to the browser UI. The build writes it to
# web/src/core/generated and the runtime data to web/public/data, where Vite picks them up.
set(HANVI_WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(_generated_dir "${HANVI_WEB_DIR}/src/core/generated")
set(_data_dir "${HANVI_WEB_DIR}/public/data")

if (NOT HANVI_OPENCC_DATA_DIR)
    message(FATAL_ERROR "Set HANVI_OPENCC_DATA_DIR to a native build's opencc folder; "
            "the OpenCC dictionaries cannot be generated when cross-compiling.")
endif ()

add_executable(HanviWasm bindings.cpp)
target_link_libraries(HanviWasm PRIVATE CoreLogic hanvi_flags)
set_target_properties(HanviWasm PROPERTIES
        OUTPUT_NAME hanvi
        SUFFIX .js
        RUNTIME_OUTPUT_DIRECTORY "${_generated_dir}")
target_link_options(HanviWasm PRIVATE
        -lembind
        "--emit-tsd=${_generated_dir}/hanvi.d.ts"
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sENVIRONMENT=worker
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=FS)

add_custom_command(TARGET HanviWasm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_data_dir}/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/dict.db" "${_data_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCC_DATA_FILES} "${_data_dir}/opencc"
        VERBATIM)
