set(MI_PLUGIN_PREFIX "media")

add_plugin(homogeneous homogeneous.cpp)
add_plugin(heterogeneous heterogeneous.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
