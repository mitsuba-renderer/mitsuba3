set(MTS_PLUGIN_PREFIX "films")

add_plugin(hdrfilm  hdrfilm.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
