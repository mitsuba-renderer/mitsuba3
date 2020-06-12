set(MTS_PLUGIN_PREFIX "samplers")

add_plugin(independent  independent.cpp)
add_plugin(stratified   stratified.cpp)
add_plugin(multijitter  multijitter.cpp)
add_plugin(orthogonal   orthogonal.cpp)
add_plugin(ldsampler    ldsampler.cpp)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
