set(MI_PLUGIN_PREFIX "sensors")

add_plugin(mradiancemeter mradiancemeter.cpp)
add_plugin(hdistant hdistant.cpp)
add_plugin(distantflux distantflux.cpp)
add_plugin(mdistant mdistant.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
