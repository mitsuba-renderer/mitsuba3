# ----------------------------------------------------------
#  Mitsuba library
# ----------------------------------------------------------

add_subdirectory(core)
add_subdirectory(render)

add_library(mitsuba SHARED)
target_link_libraries(mitsuba PUBLIC mitsuba-core)
target_link_libraries(mitsuba PUBLIC mitsuba-render)
set_target_properties(mitsuba PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU)$")
  # Treat undefined symbols as errors when linking
  target_link_libraries(mitsuba PRIVATE -Wl,--no-undefined)
endif()

# ----------------------------------------------------------
#  Mitsuba executable
# ----------------------------------------------------------

add_subdirectory(mitsuba)

# ----------------------------------------------------------
#  Plugins
# ----------------------------------------------------------

add_subdirectory(bsdfs)
add_subdirectory(emitters)
add_subdirectory(films)
add_subdirectory(integrators)
add_subdirectory(media)
add_subdirectory(phase)
add_subdirectory(rfilters)
add_subdirectory(samplers)
add_subdirectory(sensors)
add_subdirectory(shapes)
add_subdirectory(spectra)
add_subdirectory(textures)
add_subdirectory(volumes)
set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)

# ----------------------------------------------------------
#  Python bindings and extensions
# ----------------------------------------------------------

if (MI_ENABLE_PYTHON)
  add_subdirectory(python)
endif()
