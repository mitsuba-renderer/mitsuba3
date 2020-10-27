# ----------------------------------------------------------
#   Copy & installation targets for files in 'src/python'
# ----------------------------------------------------------

file(GLOB_RECURSE MTS_PYTHON_FILES RELATIVE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/*.py
  ${CMAKE_CURRENT_SOURCE_DIR}/*.shader)

set(MTS_COPY_FILES "")
foreach(file ${MTS_PYTHON_FILES})
  get_filename_component(IN_FILE_DIR ${file} DIRECTORY)
  set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${file})
  ro_copy(${IN_FILE} python/mitsuba/${file})
  install(FILES ${IN_FILE} DESTINATION python/mitsuba/${IN_FILE_DIR})
endforeach()

# ----------------------------------------------------------
#   Installation targets for auto-generated configuration
# ----------------------------------------------------------

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/../../python/mitsuba/config.py
  DESTINATION python/mitsuba
)

# ----------------------------------------------------------
#   Copy & installation targets for Enoki python bindings
# ----------------------------------------------------------

set_target_properties(enoki-python
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/python/enoki
)

install(TARGETS enoki-python
  LIBRARY DESTINATION python/enoki
)

set_target_properties(enoki-python PROPERTIES INSTALL_RPATH "${MTS_ORIGIN}/../..")

set(ENOKI_PYTHON_FILES
   __init__.py const.py detail.py generic.py
   matrix.py router.py traits.py
)

foreach(file ${ENOKI_PYTHON_FILES})
  set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/enoki/enoki/${file})
  ro_copy(${IN_FILE} python/enoki/${file})
  install(FILES ${IN_FILE} DESTINATION python/enoki)
endforeach(file)

# ----------------------------------------------------------
#   pytest.ini file
# ----------------------------------------------------------

get_filename_component(MTS_TEST_BASE ..
    REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(WRITE
  ${CMAKE_CURRENT_BINARY_DIR}/../../pytest.ini
  "[pytest]\n"
  "minversion = 3.0\n"
  "testpaths = \"${MTS_TEST_BASE}\"\n"
  "python_paths = ./python\n"
)

if (MSVC)
  ro_copy(${CMAKE_CURRENT_BINARY_DIR}/../../pytest.ini pytest.ini)
endif()


add_custom_target(copy-python ALL DEPENDS ${MTS_COPY_FILES})
set_target_properties(copy-python PROPERTIES FOLDER python)

# ----------------------------------------------------------
#   docstring target
# ----------------------------------------------------------


if (UNIX)
  include_directories(
    ${ASMJIT_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/nanogui/include
  )

  get_filename_component(MKDOC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../include/mitsuba ABSOLUTE)
  get_property(MKDOC_INCLUDE_DIRECTORIES DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
  get_property(MKDOC_COMPILE_DEFINITIONS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY COMPILE_DEFINITIONS)

  foreach (value ${MKDOC_INCLUDE_DIRECTORIES})
    list(APPEND MKDOC_CXXFLAGS -I${value})
  endforeach()

  foreach (value ${MKDOC_COMPILE_DEFINITIONS})
    list(APPEND MKDOC_CXXFLAGS -D${value})
  endforeach()

  add_custom_target(docstrings USES_TERMINAL COMMAND
    ${Python_EXECUTABLE} -m pybind11_mkdoc -std=c++17 -stdlib=libc++ -ferror-limit=100000
    ${MKDOC_CXXFLAGS}
    `find ${MKDOC_PATH}/core -name '*.h' ! -name fwd.h -print`
    `find ${MKDOC_PATH}/render -name '*.h' ! -name fwd.h -print`
    `find ${MKDOC_PATH}/ui -name '*.h' ! -name fwd.h -print`
    -o ${MKDOC_PATH}/python/docstr.h
  )
endif()
