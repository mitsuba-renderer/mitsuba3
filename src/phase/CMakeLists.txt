set(MI_PLUGIN_PREFIX "phasefunctions")

add_plugin(hg hg.cpp)
add_plugin(isotropic isotropic.cpp)
add_plugin(blendphase blendphase.cpp)
add_plugin(rayleigh rayleigh.cpp)
add_plugin(sggx sggx.cpp)
add_plugin(tabphase tabphase.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
