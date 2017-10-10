set(MTS_PLUGIN_PREFIX "samplers")

add_plugin(independent  independent.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
