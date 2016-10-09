# Make sure pytest is found or produce a fatal error
macro(mitsuba_execute_python)
  execute_process(COMMAND ${PYTHON_EXECUTABLE} -m ${ARGN} OUTPUT_QUIET ERROR_QUIET
                  RESULT_VARIABLE mitsuba_execute_python_error)
endmacro()

if(NOT PYBIND11_PYTEST_FOUND)
  mitsuba_execute_python(pytest --version --noconftest)
  if(mitsuba_execute_python_error)
    message(WARNING "Running the tests requires pytest.  Please install it manually (try: ${PYTHON_EXECUTABLE} -m pip install pytest)")
  endif()

  set(PYBIND11_PYTEST_FOUND TRUE CACHE INTERNAL "")
endif()

# A single command to compile and run the tests
add_custom_target(pytest 
  COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=${CMAKE_BINARY_DIR}/dist/python" ${PYTHON_EXECUTABLE} -m pytest -rws ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS mitsuba-python 
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist
)

# The python library depends on the test files being present in the build
# directory, so that changes in the test files will trigger the copy.
add_custom_target(python-test-files DEPENDS ${PYTHON_TEST_FILES_IN_BUILD})
add_dependencies(mitsuba-python python-test-files)

# Test targets: rely on the auto-discovery feature of python's `unittest`
add_test(NAME mitsuba-python-test
  COMMAND ${PYTHON_EXECUTABLE} -m unittest discover -v ${CMAKE_SOURCE_DIR}/src/tests
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist)

