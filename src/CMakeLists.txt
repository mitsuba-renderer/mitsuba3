add_subdirectory(libcore)
add_subdirectory(mitsuba)

if (PYTHONLIBS_FOUND)
  add_subdirectory(libpython)
endif()
