set(MI_PLUGIN_PREFIX "samplers")

add_plugin(independent  independent.cpp)
add_plugin(stratified   stratified.cpp)
add_plugin(multijitter  multijitter.cpp)
add_plugin(orthogonal   orthogonal.cpp)
add_plugin(ldsampler    ldsampler.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
