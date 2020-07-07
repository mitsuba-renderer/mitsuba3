set(MTS_PLUGIN_PREFIX "emitters")

add_plugin(area        area.cpp)
add_plugin(point       point.cpp)
add_plugin(constant    constant.cpp)
add_plugin(envmap      envmap.cpp)
add_plugin(directional directional.cpp)
add_plugin(spot        spot.cpp)
add_plugin(projector   projector.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
