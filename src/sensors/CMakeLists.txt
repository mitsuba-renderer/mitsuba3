set(MTS_PLUGIN_PREFIX "sensors")

add_plugin(perspective  perspective.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
