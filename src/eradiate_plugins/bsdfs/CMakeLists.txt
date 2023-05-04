set(MI_PLUGIN_PREFIX "bsdfs")

add_plugin(bilambertian bilambertian.cpp)
add_plugin(mqdiffuse mqdiffuse.cpp)
add_plugin(rpv rpv.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
