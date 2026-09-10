set(MI_PLUGIN_PREFIX "extremum")

add_plugin(extremum_global extremum_global.cpp)
add_plugin(extremum_grid extremum_grid.cpp)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
