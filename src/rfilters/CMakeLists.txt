set(MI_PLUGIN_PREFIX "rfilters")

add_plugin(box        box.cpp)
add_plugin(tent       tent.cpp)
add_plugin(lanczos    lanczos.cpp)
add_plugin(mitchell   mitchell.cpp)
add_plugin(catmullrom catmullrom.cpp)
add_plugin(gaussian   gaussian.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
