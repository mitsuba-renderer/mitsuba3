set(MI_PLUGIN_PREFIX "volumes")

add_plugin(sphericalcoordsvolume sphericalcoords.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
