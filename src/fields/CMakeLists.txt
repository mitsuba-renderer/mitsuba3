set(MI_PLUGIN_PREFIX "fields")

add_plugin(bitmapfield        bitmap.cpp)
add_plugin(gridfield          grid.cpp)
add_plugin(hashgridfield      hashgrid.cpp)
add_plugin(permutofield       permuto.cpp)
add_plugin(sinusoidalfield    sinusoidal.cpp)
add_plugin(neuralfield        neural.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
