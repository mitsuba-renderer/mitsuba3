if (PYTHONLIBS_FOUND)
  # Python bindings (need to include it first so that some required CMake
  # functions are defined).
  include("cmake/libpython.cmake")
endif()

# Temporarily ignore all linker errors during refactoring (CLang)
# TODO: remove this!
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-undefined,dynamic_lookup")

# # Mitsuba support libraries
add_subdirectory(libcore)
add_subdirectory(librender)
add_subdirectory(libui)

# # Mitsuba executables
# add_subdirectory(mitsuba)
add_subdirectory(mtsgui)

# # Plugins
add_subdirectory(bsdfs)
add_subdirectory(emitters)
add_subdirectory(films)
add_subdirectory(integrators)
add_subdirectory(rfilters)
add_subdirectory(samplers)
add_subdirectory(sensors)
add_subdirectory(shapes)
add_subdirectory(spectra)
add_subdirectory(textures)

if (PYTHONLIBS_FOUND)
  add_subdirectory(python)
  # Test support
  include("cmake/tests.cmake")
endif()
