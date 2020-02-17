include_directories(
  ${PYBIND11_INCLUDE_DIRS}
)

# The mitsuba-python target depends on all the mitsuba_*_ext
add_custom_target(mitsuba-python)
set_target_properties(mitsuba-python PROPERTIES FOLDER mitsuba-python)

function(add_mitsuba_python_library TARGET_NAME)
  add_library(${TARGET_NAME}-obj OBJECT
    # Common dependencies
    ${PROJECT_SOURCE_DIR}/include/mitsuba/python/python.h
    ${PROJECT_SOURCE_DIR}/include/mitsuba/python/docstr.h

    # Dependencies declared by the module
    ${ARGN}
  )

  add_library(${TARGET_NAME} SHARED $<TARGET_OBJECTS:${TARGET_NAME}-obj>)
  set_property(TARGET ${TARGET_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_property(TARGET ${TARGET_NAME}-obj PROPERTY POSITION_INDEPENDENT_CODE ON)

  add_dependencies(mitsuba-python ${TARGET_NAME})

  # The prefix and extension are provided by FindPythonLibsNew.cmake
  set_target_properties(${TARGET_NAME} PROPERTIES
    PREFIX "${PYTHON_MODULE_PREFIX}"
    SUFFIX "${PYTHON_MODULE_EXTENSION}"
  )

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
    target_link_libraries(${TARGET_NAME} PRIVATE ${PYTHON_LIBRARIES})
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

  # Copy lib${TARGET_NAME} binary to the 'dist/python/mitsuba/${NAME}' directory
  add_dist(python/mitsuba/${TARGET_NAME})

  if (APPLE)
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "@loader_path/../..")
  elseif(UNIX)
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../..")
  endif()

  set_target_properties(${TARGET_NAME} ${TARGET_NAME}-obj PROPERTIES FOLDER mitsuba-python)
endfunction()

if (MTS_ENABLE_GUI)
    # Copy nanogui python library to 'dist' folder
    add_dist(python/nanogui-python)
    if (APPLE)
    set_target_properties(nanogui-python PROPERTIES INSTALL_RPATH "@loader_path/..")
    elseif(UNIX)
    set_target_properties(nanogui-python PROPERTIES INSTALL_RPATH "$ORIGIN/..")
    endif()
    set_target_properties(nanogui-python PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# Copy enoki python library to 'dist' folder
set(ENOKI_LIBS core scalar dynamic)
if (MTS_ENABLE_OPTIX)
  set(ENOKI_LIBS ${ENOKI_LIBS} cuda cuda-autodiff)
endif()

foreach(SUFFIX IN ITEMS ${ENOKI_LIBS})
  add_dist(python/enoki/enoki-python-${SUFFIX})
  if (APPLE)
    set_target_properties(enoki-python-${SUFFIX} PROPERTIES INSTALL_RPATH "@loader_path/../..")
  elseif(UNIX)
    set_target_properties(enoki-python-${SUFFIX} PROPERTIES INSTALL_RPATH "$ORIGIN/../..")
  endif()
  set_target_properties(enoki-python-${SUFFIX} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endforeach()

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/dist/python/enoki/__init__.py
  COMMAND ${CMAKE_COMMAND} -E copy
  ${PROJECT_SOURCE_DIR}/ext/enoki/resources/__init__.py
  ${CMAKE_BINARY_DIR}/dist/python/enoki/__init__.py
)

add_custom_target(
  mitsuba-enoki-python-init
  ALL DEPENDS ${CMAKE_BINARY_DIR}/dist/python/enoki/__init__.py
)

# docstring generation support
include("cmake/docstrings.cmake")
