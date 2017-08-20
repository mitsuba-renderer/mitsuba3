# mkdoc support: automatic docstrings for the Python bindings from the C++ headers.

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

if (NOT WIN32)
  add_custom_target(mkdoc USES_TERMINAL COMMAND
    python3 ${PROJECT_SOURCE_DIR}/ext/pybind11/tools/mkdoc.py
    ${MKDOC_CXX_FLAGS_LIST}
    `find ${PROJECT_SOURCE_DIR}/include/mitsuba/core -name '*.h' ! -name fwd.h -print`
    `find ${PROJECT_SOURCE_DIR}/include/mitsuba/render -name '*.h' ! -name fwd.h -print`
    `find ${PROJECT_SOURCE_DIR}/include/mitsuba/ui -name '*.h' ! -name fwd.h -print`
    ${PROJECT_SOURCE_DIR}/ext/enoki/include/enoki/random.h
    > ${PROJECT_SOURCE_DIR}/include/mitsuba/python/docstr.h)
endif()
