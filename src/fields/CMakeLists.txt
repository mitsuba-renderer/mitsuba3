set(MI_PLUGIN_PREFIX "fields")

add_plugin(bitmap          bitmap.cpp)
add_plugin(checkerboard    checkerboard.cpp)
add_plugin(constvolume     constvolume.cpp)
add_plugin(gridvolume      gridvolume.cpp)
add_plugin(mesh_attribute  mesh_attribute.cpp)
add_plugin(sinusoidalfield sinusoidal.cpp)
add_plugin(volume          volume.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
