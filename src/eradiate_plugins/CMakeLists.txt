message(STATUS "Building Eradiate plugins")

# Plugins
add_subdirectory(bsdfs)
add_subdirectory(phase)
add_subdirectory(sensors)
add_subdirectory(volumes)

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
