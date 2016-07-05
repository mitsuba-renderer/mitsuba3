include_directories(
  ${TBB_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
)

add_definitions(
  -DMTS_BUILD_MODULE=MTS_MODULE_RENDER
)

set(INC_DIR "../../include/mitsuba/render")

add_library(mitsuba-render SHARED
  ${INC_DIR}/fwd.h
  bsdf.cpp         ${INC_DIR}/bsdf.h
  shape.cpp        ${INC_DIR}/shape.h
  mesh.cpp         ${INC_DIR}/mesh.h
  scene.cpp        ${INC_DIR}/scene.h
)

# Link to Intel's Thread Building Blocks
target_link_libraries(mitsuba-render tbb)

# Link to libcore
target_link_libraries(mitsuba-render mitsuba-core)

add_custom_command(
  TARGET mitsuba-render
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mitsuba-render> ${CMAKE_BINARY_DIR}/dist)
