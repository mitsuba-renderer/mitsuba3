if ((CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    AND NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
  # Check for Link Time Optimization support
  CHECK_CXX_COMPILER_FLAG("-flto" HAS_LTO_FLAG)
  if (HAS_LTO_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
  endif()
endif()

include_directories(
  ${PYBIND11_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
)

# Create the binding library
add_library(mitsuba-python SHARED
  main.cpp
  pcg32.cpp
  object.cpp
  thread.cpp
  logger.cpp
  appender.cpp
  formatter.cpp
)

target_link_libraries(mitsuba-python mitsuba-core)

# Don't add a 'lib' prefix to the shared library
set_target_properties(mitsuba-python PROPERTIES PREFIX "")
set_target_properties(mitsuba-python PROPERTIES OUTPUT_NAME "mitsuba")

if (WIN32)
  if (MSVC)
    # Enforce size-based optimization and link time code generation
    # on MSVC (~30% smaller binaries in experiments). /bigobj is needed
    # for bigger binding projects due to the limit to 64k addressable sections
    # /MP enables multithreaded builds (relevant when there are many files).
    set_target_properties(mitsuba-python PROPERTIES COMPILE_FLAGS "/Os /GL /MP /bigobj")
    set_target_properties(mitsuba-python PROPERTIES LINK_FLAGS "/LTCG")
  endif()

  # .PYD file extension on Windows
  set_target_properties(mitsuba-python PROPERTIES SUFFIX ".pyd")

  # Link against the Python shared library
  target_link_libraries(mitsuba-python ${PYTHON_LIBRARY})
elseif (UNIX)
  # It's quite common to have multiple copies of the same Python version
  # installed on one's system. E.g.: one copy from the OS and another copy
  # that's statically linked into an application like Blender or Maya.
  # If we link our plugin library against the OS Python here and import it
  # into Blender or Maya later on, this will cause segfaults when multiple
  # conflicting Python instances are active at the same time (even when they
  # are of the same version).

  # Windows is not affected by this issue since it handles DLL imports
  # differently. The solution for Linux and Mac OS is simple: we just don't
  # link against the Python library. The resulting shared library will have
  # missing symbols, but that's perfectly fine -- they will be resolved at
  # import time.

  # Use .so file extension on Linux/Mac OS
  set_target_properties(mitsuba-python PROPERTIES SUFFIX ".so")

  # Strip unnecessary sections from the binary on Linux/Mac OS
  if(APPLE)
    set_target_properties(mitsuba-python PROPERTIES MACOSX_RPATH ".")
    set_target_properties(mitsuba-python PROPERTIES LINK_FLAGS "-undefined dynamic_lookup ")
    if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
      add_custom_command(TARGET mitsuba-python POST_BUILD COMMAND strip -u -r ${PROJECT_BINARY_DIR}/src/libpython/mitsuba.so)
    endif()
  else()
    if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
      add_custom_command(TARGET mitsuba-python POST_BUILD COMMAND strip ${PROJECT_BINARY_DIR}/src/libpython/mitsuba.so)
    endif()
  endif()
endif()

# Compute compilation flags for 'mkdoc' target, which extracts docstrings from the C++ header files
string(REPLACE " " ";" MKDOC_CXX_FLAGS_LIST ${CMAKE_CXX_FLAGS})
get_property(MKDOC_INCLUDE_DIRECTORIES DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
get_property(MKDOC_COMPILE_DEFINITIONS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY COMPILE_DEFINITIONS)

foreach (value ${MKDOC_INCLUDE_DIRECTORIES})
  list(APPEND MKDOC_CXX_FLAGS_LIST -I${value})
endforeach()

foreach (value ${MKDOC_COMPILE_DEFINITIONS})
  list(APPEND MKDOC_CXX_FLAGS_LIST -D${value})
endforeach()

add_custom_target(mkdoc COMMAND
  python3 ${PROJECT_SOURCE_DIR}/ext/pybind11/tools/mkdoc.py 
  ${MKDOC_CXX_FLAGS_LIST}
  ${PROJECT_SOURCE_DIR}/include/mitsuba/core/*.h
  ${PROJECT_SOURCE_DIR}/ext/pcg32/*.h
  > ${CMAKE_CURRENT_SOURCE_DIR}/docstr.h)

# Copy over the test files to the build directory (detecting changes)
file(GLOB PYTHON_TEST_FILES "test_*.py")
foreach(test ${PYTHON_TEST_FILES})
  add_custom_command(
    TARGET mitsuba-python POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${test} ${CMAKE_BINARY_DIR}/src/libpython)
endforeach(test)

# Test targets: rely on the auto-discovery feature of python's `unittest`
add_test(NAME mitsuba_libpython_test COMMAND ${PYTHON_EXECUTABLE} -m unittest discover ${CMAKE_BINARY_DIR}/src/libpython -v)
add_custom_target(check ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1
        ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --verbose WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS mitsuba-python)
