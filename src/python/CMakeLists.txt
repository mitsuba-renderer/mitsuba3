# ----------------------------------------------------------
#   Copy & installation targets for files in 'src/python'
# ----------------------------------------------------------

file(GLOB_RECURSE PYTHON_FILES RELATIVE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/*.py
  ${CMAKE_CURRENT_SOURCE_DIR}/*.shader)

function (ro_copy IN_FILE OUT_FILE)
  if (UNIX)
    add_custom_command(
      OUTPUT ${OUT_FILE} DEPENDS ${IN_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE} && chmod a=r ${OUT_FILE})
  else()
    add_custom_command(
      OUTPUT ${OUT_FILE} DEPENDS ${IN_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE})
  endif()
endfunction()

foreach(file ${PYTHON_FILES})
  get_filename_component(IN_FILE_DIR ${file} DIRECTORY)
  set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${file})
  set(OUT_FILE ${MTS_BINARY_DIR}/python/mitsuba/${file})
  ro_copy(${IN_FILE} ${OUT_FILE})
  install(FILES ${IN_FILE} DESTINATION python/mitsuba/${IN_FILE_DIR})
  list(APPEND MTS_PYTHON_FILES ${OUT_FILE})
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
  set(OUT_FILE ${MTS_BINARY_DIR}/python/enoki/${file})
  ro_copy(${IN_FILE} ${OUT_FILE})
  install(FILES ${IN_FILE} DESTINATION python/enoki)
  list(APPEND MTS_PYTHON_FILES ${OUT_FILE})
endforeach(file)

add_custom_target(python-copy ALL DEPENDS ${MTS_PYTHON_FILES})
set_property(TARGET python-copy PROPERTY FOLDER misc)

# ----------------------------------------------------------
#   pytest.ini file
# ----------------------------------------------------------

get_filename_component(MTS_TEST_BASE ..
    REALPATH BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(WRITE
  ${MTS_BINARY_DIR}/pytest.ini
  "[pytest]\n"
  "minversion = 3.0\n"
  "testpaths = \"${MTS_TEST_BASE}\"\n"
  "python_paths = ./python\n"
)
