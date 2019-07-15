include_directories(
  ${NANOGUI_INCLUDE_DIRS}
)

add_definitions(
  ${NANOGUI_EXTRA_DEFS}
)

set(INC_DIR "../../include/mitsuba/ui")

add_library(mitsuba-ui-obj OBJECT
  texture.cpp
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

# Python bindings
add_subdirectory(python)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
