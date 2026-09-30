add_executable(vinson-large-file-benchmark LargeFileBenchmark.cpp)
target_link_libraries(vinson-large-file-benchmark PRIVATE vinson_editor_ui)
vinson_enable_warnings(vinson-large-file-benchmark)
set_target_properties(vinson-large-file-benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
