set(MI_PLUGIN_PREFIX "fields")

add_plugin(bitmapfield        bitmap.cpp)
add_plugin(gridfield          grid.cpp)
add_plugin(sinusoidalfield    sinusoidal.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
