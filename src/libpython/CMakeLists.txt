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

function(add_mitsuba_python_library TARGET_NAME NAME)
  set(TARGET_NAME mitsuba_${NAME}_ext)
  add_library(${TARGET_NAME}-obj OBJECT ${ARGN})

  add_library(${TARGET_NAME} SHARED $<TARGET_OBJECTS:${TARGET_NAME}-obj>)
  set_property(TARGET ${TARGET_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_property(TARGET ${TARGET_NAME}-obj PROPERTY POSITION_INDEPENDENT_CODE ON)

  # The prefix and extension are provided by FindPythonLibsNew.cmake
  set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "${PYTHON_MODULE_PREFIX}")
  set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX "${PYTHON_MODULE_EXTENSION}")

  if (WIN32)
    if (MSVC)
      # /MP enables multithreaded builds (relevant when there are many files), /bigobj is
      # needed for bigger binding projects due to the limit to 64k addressable sections
      set_property(TARGET ${TARGET_NAME} APPEND PROPERTY COMPILE_OPTIONS /MP /bigobj)

      # Enforce size-based optimization in release modes
      set_property(TARGET ${TARGET_NAME} APPEND PROPERTY COMPILE_OPTIONS
        "$<$<CONFIG:Release>:/Os>" "$<$<CONFIG:MinSizeRel>:/Os>" "$<$<CONFIG:RelWithDebInfo>:/Os>")
    endif()

    # Link against the Python shared library
    target_link_libraries(${TARGET_NAME} ${PYTHON_LIBRARY})
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
      set_target_properties(${TARGET_NAME} PROPERTIES MACOSX_RPATH ".")
      set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup ")
      if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND strip -u -r $<TARGET_FILE:${TARGET_NAME}>)
      endif()
    else()
      if (NOT ${U_CMAKE_BUILD_TYPE} MATCHES DEBUG)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD COMMAND strip $<TARGET_FILE:${TARGET_NAME}>)
      endif()
    endif()
  endif()

  # Create the 'dist/python' directory
  add_custom_command(
    TARGET ${TARGET_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist/python/mitsuba/${NAME}"
  )

  # Copy lib${TARGET_NAME} binary to the 'dist/python' directory
  add_custom_command(
    TARGET ${TARGET_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/dist/python/mitsuba/${NAME})

  if (APPLE)
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "@loader_path/..")
  endif()
endfunction()

add_mitsuba_python_library(mitsuba_core_ext core
  # main
  python.h
  docstr.h
  mitsuba_core_ext.cpp

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
)

add_mitsuba_python_library(mitsuba_render_ext render
  mitsuba_render_ext.cpp
  scene.cpp
  shape.cpp
)

add_mitsuba_python_library(mitsuba_ui_ext ui
  mitsuba_ui_ext.cpp
)

cmake_policy(SET CMP0026 OLD)
get_target_property(fname nanogui-python LOCATION_RELEASE)
get_filename_component(fname ${fname} NAME)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/dist/python/${fname}
  DEPENDS $<TARGET_FILE:nanogui-python>
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nanogui-python> ${CMAKE_BINARY_DIR}/dist/python/${fname}
)
add_custom_target(copy-nanogui-python ALL DEPENDS ${CMAKE_BINARY_DIR}/dist/python/${fname})

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
  `find ${PROJECT_SOURCE_DIR}/include/mitsuba/ui -name '*.h' ! -name fwd.h -print`
  ${PROJECT_SOURCE_DIR}/ext/pcg32/*.h
  > ${CMAKE_CURRENT_SOURCE_DIR}/docstr.h)

if (APPLE)
  set_target_properties(nanogui-python PROPERTIES INSTALL_RPATH "@loader_path/..")
endif()

target_link_libraries(mitsuba_core_ext PRIVATE mitsuba-core)
target_link_libraries(mitsuba_render_ext PRIVATE mitsuba-core mitsuba-render)
target_link_libraries(mitsuba_ui_ext PRIVATE mitsuba-core mitsuba-render mitsuba-ui)

add_custom_target(copy_python_files ALL DEPENDS ${PYTHON_FILES_OUT})
