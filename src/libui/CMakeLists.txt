include_directories(
  ${NANOGUI_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(
  ${NANOGUI_EXTRA_DEFS}
)

# Glob up resource files
file(GLOB resources "${PROJECT_SOURCE_DIR}/resources/*.png")

# Concatenate resource files into a comma separated string
string (REGEX REPLACE "([^\\]|^);" "\\1," resources_string "${resources}")
string (REGEX REPLACE "[\\](.)" "\\1" resources_string "${resources_string}")

# Create command line for running bin2c cmake script
set(bin2c_cmdline
  -DOUTPUT_C=libui_resources.cpp
  -DOUTPUT_H=libui_resources.h
  "-DINPUT_FILES=${resources_string}"
  -P "${PROJECT_SOURCE_DIR}/ext/nanogui/resources/bin2c.cmake")

# Run bin2c on resource files
add_custom_command(
  OUTPUT libui_resources.cpp libui_resources.h
  COMMAND ${CMAKE_COMMAND} ARGS ${bin2c_cmdline}
  DEPENDS ${resources}
  COMMENT "Running bin2c"
  PRE_BUILD VERBATIM)

set(INC_DIR "../../include/mitsuba/ui")

add_library(mitsuba-ui-obj OBJECT
  texture.cpp ${INC_DIR}/texture.h
  viewer.cpp  ${INC_DIR}/viewer.h
  libui_resources.cpp libui_resources.h
)

add_library(mitsuba-ui SHARED $<TARGET_OBJECTS:mitsuba-ui-obj>)
set_property(TARGET mitsuba-ui-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(mitsuba-ui-obj mitsuba-ui PROPERTIES FOLDER mitsuba-ui)
target_compile_definitions(mitsuba-ui-obj PRIVATE -DMTS_BUILD_MODULE=MTS_MODULE_UI ${NANOGUI_EXTRA_DEFS})

# Link to Mitsuba core library
target_link_libraries(mitsuba-ui PUBLIC mitsuba-core)

# Link to NanoGUI
target_link_libraries(mitsuba-ui PUBLIC nanogui)
set_target_properties(mitsuba-ui PROPERTIES FOLDER mitsuba-ui)

# Copy to 'dist' directory
add_dist(mitsuba-ui nanogui)

# # Python bindings
#add_subdirectory(python)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
