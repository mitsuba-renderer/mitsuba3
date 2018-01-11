set(MTS_PLUGIN_PREFIX "emitters")

add_plugin(area      area.cpp)
add_plugin(point     point.cpp)
add_plugin(constant  constant.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
