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
  add_subdirectory(libpython)
  add_subdirectory(python)

  # Testcases
  add_subdirectory(tests)
endif()
