# Make sure pytest is found or produce a fatal error
if (NOT MITSUBA_PYTEST_FOUND)
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version --noconftest OUTPUT_QUIET ERROR_QUIET
                  RESULT_VARIABLE MITSUBA_EXEC_PYTHON_ERR)
  if(MITSUBA_EXEC_PYTHON_ERR)
    message(FATAL_ERROR "Running the tests requires pytest.  Please install it manually (try: ${PYTHON_EXECUTABLE} -m pip install pytest pytest-xdist)")
  endif()
  set(MITSUBA_PYTEST_FOUND TRUE CACHE INTERNAL "")
endif()

# A single command to compile and run the tests
add_custom_target(pytest 
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_BINARY_DIR}/dist/python" ${PYTHON_EXECUTABLE} -m pytest -n auto -rws ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS mitsuba-python 
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist
  USES_TERMINAL
)
