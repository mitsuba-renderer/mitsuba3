set(MTS_PLUGIN_PREFIX "bsdfs")

add_plugin(diffuse diffuse.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
