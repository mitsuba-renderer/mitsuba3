set(MI_PLUGIN_PREFIX "extrema")

add_plugin(gridextremum gridextremum.cpp)
add_plugin(globalextremum globalextremum.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
