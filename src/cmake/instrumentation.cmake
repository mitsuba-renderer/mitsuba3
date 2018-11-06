if (MTS_ENABLE_PROFILER)

  # find_package(ITT)
  # message(STATUS "ITT: ${ITT_INCLUDE_DIRS}")

  # TODO: clone in ext/ and use https://github.com/intel/IntelSEAPI
  # set(ITT_INCLUDE_DIRS "/Users/merlin/Code/intel/IntelSEAPI/ittnotify/include")
  # set(ITT_LIBRARY_DIRS "/Users/merlin/Code/intel/bin/")
  # set(ITT_LIBRARIES "libittnotify.a")

  # include_directories(${ITT_INCLUDE_DIRS})
  # link_directories(${ITT_LIBRARY_DIRS})

  # message(STATUS "Using Intel's ITT instrumentation library.")

endif()
