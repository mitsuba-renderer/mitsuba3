if ((CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    AND NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
  # Check for Link Time Optimization support
  CHECK_CXX_COMPILER_FLAG("-flto" HAS_LTO_FLAG)

  # Disabled for now for faster compile times
  #if (HAS_LTO_FLAG)
  #  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
  #endif()
endif()

include_directories(
  ${PYBIND11_INCLUDE_DIRS}
  ${EIGEN_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
  ${TINYFORMAT_INCLUDE_DIRS}
)

# Create the binding library
add_library(mitsuba-python SHARED
  appender.cpp
  atomic.cpp
  filesystem.cpp
  formatter.cpp
  logger.cpp
  main.cpp
  math.cpp
  util.cpp
  object.cpp
  pcg32.cpp
  properties.cpp
  python.h
  thread.cpp
  argparser.cpp
  xml.cpp
  fresolver.cpp
)

target_link_libraries(mitsuba-python mitsuba-core)

# Don't add a 'lib' prefix to the shared library
set_target_properties(mitsuba-python PROPERTIES PREFIX "")
set_target_properties(mitsuba-python PROPERTIES OUTPUT_NAME "mitsuba")

if (WIN32)
  if (MSVC)
    # /MP enables multithreaded builds (relevant when there are many files), /bigobj is
    # needed for bigger binding projects due to the limit to 64k addressable sections
    set_property(TARGET mitsuba-python APPEND PROPERTY COMPILE_OPTIONS /MP /bigobj)
    # Enforce size-based optimization and link time code generation on MSVC
    # (~30% smaller binaries in experiments); do nothing in debug mode.
    set_property(TARGET mitsuba-python APPEND PROPERTY COMPILE_OPTIONS
      "$<$<CONFIG:Release>:/Os>" "$<$<CONFIG:Release>:/GL>"
      "$<$<CONFIG:MinSizeRel>:/Os>" "$<$<CONFIG:MinSizeRel>:/GL>"
      "$<$<CONFIG:RelWithDebInfo>:/Os>" "$<$<CONFIG:RelWithDebInfo>:/GL>")
    set_property(TARGET mitsuba-python APPEND_STRING PROPERTY LINK_FLAGS_RELEASE "/LTCG ")
    set_property(TARGET mitsuba-python APPEND_STRING PROPERTY LINK_FLAGS_MINSIZEREL "/LTCG ")
    set_property(TARGET mitsuba-python APPEND_STRING PROPERTY LINK_FLAGS_RELWITHDEBINFO "/LTCG ")
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

# Create the 'dist/python' directory 
add_custom_command(
  TARGET mitsuba-python
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist/python"
)

# Copy libmitsuba-python binary to the 'dist/python' directory
add_custom_command(
  TARGET mitsuba-python
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mitsuba-python> ${CMAKE_BINARY_DIR}/dist/python)

# Copy nanogui python binding binary to the 'dist/python' directory
add_custom_command(
  TARGET mitsuba-python
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nanogui_python> ${CMAKE_BINARY_DIR}/dist/python)

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
  `find ${PROJECT_SOURCE_DIR}/include/mitsuba/core -name '*.h' ! -name fwd.h -print`
  ${PROJECT_SOURCE_DIR}/ext/pcg32/*.h
  > ${CMAKE_CURRENT_SOURCE_DIR}/docstr.h)

# The python library depends on the test files being present in the build
# directory, so that changes in the test files will trigger the copy.
add_custom_target(python-test-files DEPENDS ${PYTHON_TEST_FILES_IN_BUILD})
add_dependencies(mitsuba-python python-test-files)

# Test targets: rely on the auto-discovery feature of python's `unittest`
add_test(NAME mitsuba-python-test
  COMMAND ${PYTHON_EXECUTABLE} -m unittest discover -v ${CMAKE_SOURCE_DIR}/src/tests
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist/python)

add_custom_target(check ${CMAKE_COMMAND} -E env CTEST_OUTPUT_ON_FAILURE=1
  ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --verbose
  DEPENDS mitsuba-python)

if (APPLE)
  set_target_properties(mitsuba-python PROPERTIES INSTALL_RPATH "@loader_path/..")
  set_target_properties(nanogui_python PROPERTIES INSTALL_RPATH "@loader_path/..")
endif()
