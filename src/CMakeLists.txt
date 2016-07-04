# Mitsuba support libraries

add_subdirectory(libcore)
add_subdirectory(librender)

if (PYTHONLIBS_FOUND)
  add_subdirectory(libpython)
endif()

# Mitsuba executables
add_subdirectory(mitsuba)

# Plugins
add_subdirectory(bsdfs)
add_subdirectory(shapes)
