set(MTS_PLUGIN_PREFIX "sensors")

add_plugin(perspective     perspective.cpp)
add_plugin(radiancemeter   radiancemeter.cpp)
add_plugin(thinlens        thinlens.cpp)
add_plugin(irradiancemeter irradiancemeter.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
