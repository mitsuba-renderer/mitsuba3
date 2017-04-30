# Test support in mitsuba using pytest and pytest-xdist

# Make sure pytest and the pytest-xdist plugin are both found or produce a fatal error
if (NOT MITSUBA_PYTEST_FOUND)
execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version --noconftest OUTPUT_QUIET ERROR_QUIET
                RESULT_VARIABLE MITSUBA_EXEC_PYTHON_ERR_1)
  if (MITSUBA_EXEC_PYTHON_ERR_1)
    message(FATAL_ERROR "Running the tests requires pytest.  Please install it manually (try: ${PYTHON_EXECUTABLE} -m pip install 'pytest>=3.0.0' pytest-xdist)")
  endif()
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version -n 1 --noconftest OUTPUT_QUIET ERROR_QUIET
                  RESULT_VARIABLE MITSUBA_EXEC_PYTHON_ERR_2)
  if (NOT WIN32 AND MITSUBA_EXEC_PYTHON_ERR_2)
    message(FATAL_ERROR "Running the tests requires pytest-xdist (in addition to pytest).  Please install it manually (try: ${PYTHON_EXECUTABLE} -m pip install pytest-xdist)")
  endif()
  set(MITSUBA_PYTEST_FOUND TRUE CACHE INTERNAL "")
endif()

include(ProcessorCount)
ProcessorCount(PROC_COUNT)

# Don't use more than 8 processors to run tests
if (PROC_COUNT EQUAL 0)
  set(PROC_COUNT auto)
elseif (PROC_COUNT GREATER 8)
  set(PROC_COUNT 8)
endif()

if (WIN32)
  # pytest-xdist does not seem to run reliably on windows
  set(PYTEST_XDIST "-v")
else()
  set(PYTEST_XDIST "-n ${PROC_COUNT}")
endif()

# A single command to compile and run the tests
add_custom_target(pytest
  COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/dist" "PYTHONPATH=${CMAKE_BINARY_DIR}/dist/python" ${PYTHON_EXECUTABLE} -m pytest ${PYTEST_XDIST} -rws ${MITSUBA_TEST_DIRECTORIES}
  DEPENDS mitsuba-python python-copy
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist
  USES_TERMINAL
)
