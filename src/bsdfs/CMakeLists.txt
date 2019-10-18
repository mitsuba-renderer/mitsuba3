set(MTS_PLUGIN_PREFIX "bsdfs")

add_plugin(conductor conductor.cpp)
add_plugin(dielectric dielectric.cpp)
add_plugin(diffuse diffuse.cpp)
add_plugin(measured measured.cpp)
add_plugin(plastic plastic.cpp)
add_plugin(roughconductor roughconductor.cpp)
add_plugin(roughdielectric roughdielectric.cpp)
add_plugin(roughplastic roughplastic.cpp)
add_plugin(thindielectric thindielectric.cpp)
add_plugin(twosided twosided.cpp)

# TODO: fix "ambiguous call to eval()" issue
# add_plugin(blendbsdf blendbsdf.cpp)
# add_plugin(mask mask.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
