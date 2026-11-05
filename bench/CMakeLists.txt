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

# The data it loads by default sits next to it, as for the apps. Copied here rather than through
# Deploy so the benchmark builds without Qt.
add_custom_command(TARGET HanviBench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:HanviBench>/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCC_DATA_FILES} "$<TARGET_FILE_DIR:HanviBench>/opencc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/dict.db" "$<TARGET_FILE_DIR:HanviBench>"
        VERBATIM)
if (TARGET Dictionaries)
    add_dependencies(HanviBench Dictionaries)
endif ()
