set(MTS_PLUGIN_PREFIX "shapes")

add_plugin(obj    obj.cpp)
add_plugin(ply    ply.cpp)
add_plugin(rectangle rectangle.cpp)
add_plugin(sphere sphere.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
