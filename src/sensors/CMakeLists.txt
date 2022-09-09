set(MI_PLUGIN_PREFIX "sensors")

add_plugin(perspective     perspective.cpp)
add_plugin(orthographic    orthographic.cpp)
add_plugin(radiancemeter   radiancemeter.cpp)
add_plugin(thinlens        thinlens.cpp)
add_plugin(irradiancemeter irradiancemeter.cpp)
add_plugin(distant         distant.cpp)
add_plugin(batch           batch.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
