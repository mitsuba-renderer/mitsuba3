set(MI_PLUGIN_PREFIX "textures")

add_plugin(bitmap         bitmap.cpp)
add_plugin(checkerboard   checkerboard.cpp)

# The target name avoids a clash with an Embree-internal "math" target
add_plugin(math_tex       math.cpp)
set_target_properties(math_tex PROPERTIES OUTPUT_NAME math)

add_plugin(mesh_attribute mesh_attribute.cpp)
add_plugin(volume         volume.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
