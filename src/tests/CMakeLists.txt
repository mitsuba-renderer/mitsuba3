# Make sure pytest is found or produce a fatal error
if (NOT MITSUBA_PYTEST_FOUND)
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version --noconftest OUTPUT_QUIET ERROR_QUIET
                  RESULT_VARIABLE MITSUBA_EXEC_PYTHON_ERR_1)
  if(MITSUBA_EXEC_PYTHON_ERR_1)
    message(FATAL_ERROR "Running the tests requires pytest.  Please install it manually (try: ${PYTHON_EXECUTABLE} -m pip install pytest pytest-xdist)")
  endif()
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version -n 1 --noconftest OUTPUT_QUIET ERROR_QUIET
                  RESULT_VARIABLE MITSUBA_EXEC_PYTHON_ERR_2)
  if(MITSUBA_EXEC_PYTHON_ERR_2)
    message(FATAL_ERROR "Running the tests requires pytest-xdist (in addition to pytest).  Please install it manually (try: ${PYTHON_EXECUTABLE} -m pip install pytest-xdist)")
  endif()
  set(MITSUBA_PYTEST_FOUND TRUE CACHE INTERNAL "")
endif()

include(ProcessorCount)
ProcessorCount(PROC_COUNT)

if (PROC_COUNT EQUAL 0)
  set(PROC_COUNT auto)
elseif (PROC_COUNT GREATER 8)
  set(PROC_COUNT 8)
endif()

# A single command to compile and run the tests
add_custom_target(pytest 
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_BINARY_DIR}/dist/python" ${PYTHON_EXECUTABLE} -m pytest -n ${PROC_COUNT} -rws ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS mitsuba-python 
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist
  USES_TERMINAL
)
