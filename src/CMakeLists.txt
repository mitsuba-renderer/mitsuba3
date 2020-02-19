if (MTS_ENABLE_PYTHON)
  # Python bindings (need to include it first so that some required CMake
  # functions are defined).
  include("cmake/libpython.cmake")
endif()

# Mitsuba support libraries
add_subdirectory(libcore)
add_subdirectory(librender)
if (MTS_ENABLE_GUI)
    add_subdirectory(libui)
endif()

# Mitsuba executables
add_subdirectory(mitsuba)

if (MTS_ENABLE_GUI)
    add_subdirectory(mtsgui)
endif()

# # Plugins
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

if (MTS_ENABLE_PYTHON)
  add_subdirectory(python)

  # Test support
  include("cmake/tests.cmake")
endif()
