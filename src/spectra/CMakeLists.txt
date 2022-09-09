set(MI_PLUGIN_PREFIX "spectra")

add_plugin(blackbody blackbody.cpp)
add_plugin(uniform uniform.cpp)
add_plugin(regular regular.cpp)
add_plugin(irregular irregular.cpp)
add_plugin(d65 d65.cpp)
add_plugin(srgb srgb.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
