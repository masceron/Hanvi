add_executable(HanviCLI main.cpp)

set_target_properties(HanviCLI PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(HanviCLI PRIVATE
        CoreLogic
        Qt::Core
        Qt::Concurrent
        hanvi_flags
)

