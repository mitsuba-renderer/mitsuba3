set(MI_PLUGIN_PREFIX "fields")

add_plugin(bitmapfield        bitmap.cpp)
add_plugin(gridfield          grid.cpp)
add_plugin(hashgridencoding   hashgrid.cpp)
add_plugin(permutoencoding    permuto.cpp)
add_plugin(sinusoidalencoding sinusoidal.cpp)
add_plugin(neuralfield        neural.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
