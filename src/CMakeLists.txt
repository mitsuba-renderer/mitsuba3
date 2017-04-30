if (PYTHONLIBS_FOUND)
  # Python bindings (need to include it first so that some required CMake
  # functions are defined).
  include("cmake/libpython.cmake")
endif()

# Mitsuba support libraries
add_subdirectory(libcore)
add_subdirectory(librender)
add_subdirectory(libui)

# Mitsuba executables
add_subdirectory(mitsuba)

# Plugins
add_subdirectory(spectra)
add_subdirectory(bsdfs)
add_subdirectory(shapes)
add_subdirectory(rfilters)

if (PYTHONLIBS_FOUND)
  add_subdirectory(python)
  # Test support
  include("cmake/tests.cmake")
endif()
