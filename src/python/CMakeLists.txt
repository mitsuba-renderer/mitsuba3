file(GLOB_RECURSE PYTHON_FILES RELATIVE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/*.py
  ${CMAKE_CURRENT_SOURCE_DIR}/*.shader)

foreach(FNAME ${PYTHON_FILES})
  set(IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FNAME})
  set(OUT_FILE ${CMAKE_BINARY_DIR}/dist/python/mitsuba/${FNAME})
  if (UNIX)
    add_custom_command(
      OUTPUT ${OUT_FILE} DEPENDS ${IN_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE} && chmod a=r ${OUT_FILE})
  else()
    add_custom_command(
      OUTPUT ${OUT_FILE} DEPENDS ${IN_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy ${IN_FILE} ${OUT_FILE})
  endif()
  set(PYTHON_OUTPUT ${PYTHON_OUTPUT} ${OUT_FILE})
endforeach()

add_custom_target(python-copy ALL DEPENDS ${PYTHON_OUTPUT})
set_property(TARGET python-copy PROPERTY FOLDER misc)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/dist/python/mitsuba)