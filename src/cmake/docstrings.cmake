# Automatic generation of docstrings for the Python bindings from the C++ headers.

# Compute compilation flags for 'docstrings' target, which extracts docstrings from the C++ header files
if (NOT WIN32)
  string(REPLACE " " ";" MKDOC_CXX_FLAGS_LIST ${CMAKE_CXX_FLAGS})
  get_property(MKDOC_INCLUDE_DIRECTORIES DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
  get_property(MKDOC_COMPILE_DEFINITIONS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY COMPILE_DEFINITIONS)

  foreach (value ${MKDOC_INCLUDE_DIRECTORIES})
    list(APPEND MKDOC_CXX_FLAGS_LIST -I${value})
  endforeach()
  list(APPEND MKDOC_CXX_FLAGS_LIST -I${ZMQ_INCLUDE_DIR})

  foreach (value ${MKDOC_COMPILE_DEFINITIONS})
    list(APPEND MKDOC_CXX_FLAGS_LIST -D${value})
  endforeach()

  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=include
                    ERROR_QUIET OUTPUT_VARIABLE CLANG_RESOURCE_DIR)
    get_filename_component(CLANG_RESOURCE_DIR ${CLANG_RESOURCE_DIR} DIRECTORY)
    if (CLANG_RESOURCE_DIR)
      list(APPEND MKDOC_CXX_FLAGS_LIST "-resource-dir=${CLANG_RESOURCE_DIR}")
    endif()
  endif()

  list(REMOVE_ITEM MKDOC_CXX_FLAGS_LIST "-fno-fat-lto-objects")
  list(REMOVE_ITEM MKDOC_CXX_FLAGS_LIST "-flto")

  add_custom_target(docstrings USES_TERMINAL COMMAND
    python3 ${PROJECT_SOURCE_DIR}/ext/pybind11/tools/mkdoc.py
    ${MKDOC_CXX_FLAGS_LIST} -DNANOGUI_USE_OPENGL
    -Wno-pragma-once-outside-header
    -ferror-limit=100000
    `find ${PROJECT_SOURCE_DIR}/include/mitsuba/core -name '*.h' ! -name fwd.h -print`
    `find ${PROJECT_SOURCE_DIR}/include/mitsuba/render -name '*.h' ! -name fwd.h -print`
    `find ${PROJECT_SOURCE_DIR}/include/mitsuba/ui -name '*.h' ! -name fwd.h -print`
    > ${PROJECT_SOURCE_DIR}/include/mitsuba/python/docstr.h)
endif()
