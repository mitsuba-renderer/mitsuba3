set(MTS_PLUGIN_PREFIX "bsdfs")

add_plugin(diffuse diffuse.cpp)
add_plugin(dielectric dielectric.cpp)
add_plugin(roughconductor roughconductor.cpp)
add_plugin(twosided twosided.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
