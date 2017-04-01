set(INC_DIR "../../include/mitsuba/render")

add_library(mitsuba-render-obj OBJECT
  ${INC_DIR}/fwd.h
  ${INC_DIR}/records.h

  bsdf.cpp         ${INC_DIR}/bsdf.h
  common.cpp       ${INC_DIR}/common.h
  kdtree.cpp       ${INC_DIR}/kdtree.h
  mesh.cpp         ${INC_DIR}/mesh.h
  scene.cpp        ${INC_DIR}/scene.h
  shape.cpp        ${INC_DIR}/shape.h
)

add_library(mitsuba-render SHARED $<TARGET_OBJECTS:mitsuba-render-obj>)
set_property(TARGET mitsuba-render-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(mitsuba-render-obj mitsuba-render PROPERTIES FOLDER mitsuba-render)
target_compile_definitions(mitsuba-render-obj PRIVATE -DMTS_BUILD_MODULE=MTS_MODULE_RENDER)

# Link to Intel's Thread Building Blocks
target_link_libraries(mitsuba-render PRIVATE tbb)

# Link to libcore
target_link_libraries(mitsuba-render PUBLIC mitsuba-core)

# Copy to 'dist' directory
add_dist(mitsuba-render)

# Python bindings
add_subdirectory(python)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
