set(MTS_PLUGIN_PREFIX "media")

add_plugin(homogeneous homogeneous.cpp)
add_plugin(heterogeneous heterogeneous.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
