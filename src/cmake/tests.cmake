# Test support in mitsuba using pytest and pytest-xdist

# Make sure pytest and the pytest-xdist plugin are both found or produce a fatal error
if (NOT MITSUBA_PYTEST_FOUND)
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version --noconftest OUTPUT_QUIET ERROR_QUIET
                  RESULT_VARIABLE MITSUBA_EXEC_PYTHON_ERR_1)
  if (MITSUBA_EXEC_PYTHON_ERR_1)
    message(WARNING "Running the tests requires pytest.  Please install it manually: \n$ ${PYTHON_EXECUTABLE} -m pip install pytest pytest-xdist")
  else()
    execute_process(COMMAND ${PYTHON_EXECUTABLE} -m pytest --version -n 1 --noconftest OUTPUT_QUIET ERROR_QUIET
                    RESULT_VARIABLE MITSUBA_EXEC_PYTHON_ERR_2)
    if (NOT WIN32 AND MITSUBA_EXEC_PYTHON_ERR_2)
      message(WARNING "Running the tests requires pytest-xdist.  Please install it manually: \n$ ${PYTHON_EXECUTABLE} -m pip install pytest-xdist")
    else()
      set(MITSUBA_PYTEST_FOUND TRUE CACHE INTERNAL "")
    endif()
  endif()
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

if (MITSUBA_PYTEST_FOUND)
  # A single command to compile and run the tests
  add_custom_target(pytest
    COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:${CMAKE_BINARY_DIR}/dist" "PYTHONPATH=${CMAKE_BINARY_DIR}/dist/python" ${PYTHON_EXECUTABLE} -m pytest ${PYTEST_XDIST} -rws ${MITSUBA_TEST_DIRECTORIES}
    DEPENDS mitsuba-python python-copy dist-copy
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist
    USES_TERMINAL
  )
endif()
