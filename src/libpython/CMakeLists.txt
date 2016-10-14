include_directories(
  ${PYBIND11_INCLUDE_DIRS}
  ${EIGEN_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
  ${TINYFORMAT_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIR}
  ${HYPOTHESIS_INCLUDE_DIRS}
  ${NANOGUI_INCLUDE_DIRS}
)

# Create the binding library
add_library(mitsuba-python-obj OBJECT
  # main
  python.h
  docstr.h
  main.cpp

  # ext
  pcg32.cpp

  # libcore
  appender.cpp
  atomic.cpp
  filesystem.cpp
  formatter.cpp
  logger.cpp
  math.cpp
  util.cpp
  object.cpp
  properties.cpp
  python.h
  stream.cpp
  thread.cpp
  argparser.cpp
  xml.cpp
  fresolver.cpp
  vector.cpp
  bbox.cpp
  frame.cpp
  ray.cpp
  struct.cpp
  bitmap.cpp
  warp.cpp

  # librender
  scene.cpp
  shape.cpp

  # libui
  #warp_visualizer.cpp
)

add_library(mitsuba-python SHARED $<TARGET_OBJECTS:mitsuba-python-obj>)
set_property(TARGET mitsuba-python PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET mitsuba-python-obj PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(mitsuba-python PRIVATE mitsuba-core mitsuba-render mitsuba-ui nanogui)

# Don't add a 'lib' prefix to the shared library
set_target_properties(mitsuba-python PROPERTIES OUTPUT_NAME "mitsuba")

# The prefix and extension are provided by FindPythonLibsNew.cmake
set_target_properties(mitsuba-python PROPERTIES PREFIX "${PYTHON_MODULE_PREFIX}")
set_target_properties(mitsuba-python PROPERTIES SUFFIX "${PYTHON_MODULE_EXTENSION}")

if (WIN32)
  if (MSVC)
    # /MP enables multithreaded builds (relevant when there are many files), /bigobj is
    # needed for bigger binding projects due to the limit to 64k addressable sections
    set_property(TARGET mitsuba-python APPEND PROPERTY COMPILE_OPTIONS /MP /bigobj)

    # Enforce size-based optimization in release modes
    set_property(TARGET mitsuba-python APPEND PROPERTY COMPILE_OPTIONS
      "$<$<CONFIG:Release>:/Os>" "$<$<CONFIG:MinSizeRel>:/Os>" "$<$<CONFIG:RelWithDebInfo>:/Os>")
  endif()

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

  # Strip unnecessary sections from the binary on Linux/Mac OS
  if(APPLE)
    set_target_properties(mitsuba-python PROPERTIES MACOSX_RPATH ".")
    set_target_properties(mitsuba-python PROPERTIES LINK_FLAGS "-undefined dynamic_lookup ")
    if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
      add_custom_command(TARGET python-python POST_BUILD COMMAND strip -u -r $<TARGET_FILE:python-python>)
    endif()
  else()
    if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
      add_custom_command(TARGET mitsuba-python POST_BUILD COMMAND strip $<TARGET_FILE:mitsuba-python>)
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
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nanogui-python> ${CMAKE_BINARY_DIR}/dist/python)

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
  `find ${PROJECT_SOURCE_DIR}/include/mitsuba/render -name '*.h' ! -name fwd.h -print`
  `find ${PROJECT_SOURCE_DIR}/include/mitsuba/gui -name '*.h' ! -name fwd.h -print`
  ${PROJECT_SOURCE_DIR}/ext/pcg32/*.h
  > ${CMAKE_CURRENT_SOURCE_DIR}/docstr.h)

if (APPLE)
  set_target_properties(mitsuba-python PROPERTIES INSTALL_RPATH "@loader_path/..")
  set_target_properties(nanogui-python PROPERTIES INSTALL_RPATH "@loader_path/..")
endif()
