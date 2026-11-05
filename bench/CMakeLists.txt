add_executable(HanviBench bench.cpp)

set_target_properties(HanviBench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(HanviBench PRIVATE
        CoreLogic
        hanvi_flags
)
if (WIN32)
    target_link_libraries(HanviBench PRIVATE psapi)
endif ()

add_custom_command(TARGET HanviBench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:HanviBench>/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCC_DATA_FILES} "$<TARGET_FILE_DIR:HanviBench>/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/dict.db" "$<TARGET_FILE_DIR:HanviBench>"
        VERBATIM)
if (TARGET Dictionaries)
    add_dependencies(HanviBench Dictionaries)
endif ()

if (HANVI_PGO_TRAINING)
    set(_raw_profile "${CMAKE_BINARY_DIR}/pgo/bench.profraw")
    add_custom_target(PgoProfile
            COMMAND ${CMAKE_COMMAND} -E rm -f "${_raw_profile}"
            COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${_raw_profile}"
                    "$<TARGET_FILE:HanviBench>" --rounds 1 "${HANVI_PGO_TRAINING}"
            COMMAND "${LLVM_PROFDATA_EXECUTABLE}" merge -o "${CMAKE_BINARY_DIR}/hanvi.profdata" "${_raw_profile}"
            DEPENDS HanviBench
            COMMENT "Training the PGO profile on ${HANVI_PGO_TRAINING}"
            VERBATIM)
endif ()
