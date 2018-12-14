set(INC_DIR "../../include/mitsuba/render")

include_directories(
  ${RGB2SPEC_INCLUDE_DIRS}
)

add_library(mitsuba-render-obj OBJECT
  ${INC_DIR}/fwd.h
  ${INC_DIR}/ior.h
  ${INC_DIR}/microfacet.h
  ${INC_DIR}/records.h

  bsdf.cpp         ${INC_DIR}/bsdf.h
  common.cpp       ${INC_DIR}/common.h
  emitter.cpp      ${INC_DIR}/emitter.h
  endpoint.cpp     ${INC_DIR}/endpoint.h
  film.cpp         ${INC_DIR}/film.h
  imageblock.cpp   ${INC_DIR}/imageblock.h
  integrator.cpp   ${INC_DIR}/integrator.h
  interaction.cpp  ${INC_DIR}/interaction.h
  kdtree.cpp       ${INC_DIR}/kdtree.h
  medium.cpp       ${INC_DIR}/medium.h
  mesh.cpp         ${INC_DIR}/mesh.h
  microfacet.cpp   ${INC_DIR}/microfacet.h
  rtbench.cpp      ${INC_DIR}/rtbench.h
  sampler.cpp      ${INC_DIR}/sampler.h
  scene.cpp        ${INC_DIR}/scene.h
  sensor.cpp       ${INC_DIR}/sensor.h
  shape.cpp        ${INC_DIR}/shape.h
  spiral.cpp       ${INC_DIR}/spiral.h
  srgb.cpp         ${INC_DIR}/srgb.h
  fresnel.cpp      ${INC_DIR}/fresnel.h
  mueller.cpp      ${INC_DIR}/mueller.h
  spectrum.cpp     ${INC_DIR}/spectrum.h
)

add_library(mitsuba-render SHARED $<TARGET_OBJECTS:mitsuba-render-obj>)
set_property(TARGET mitsuba-render-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(mitsuba-render-obj mitsuba-render PROPERTIES FOLDER mitsuba-render)
target_compile_definitions(mitsuba-render-obj PRIVATE -DMTS_BUILD_MODULE=MTS_MODULE_RENDER)

target_link_libraries(mitsuba-render
  # Link to rgb2spec upsampling library
  PRIVATE rgb2spec
)

# Link to Intel's Thread Building Blocks
target_link_libraries(mitsuba-render PRIVATE tbb)

# Link to libcore
target_link_libraries(mitsuba-render PUBLIC mitsuba-core)

# Link to Embree
if (MTS_USE_EMBREE)
  if (APPLE)
      target_link_libraries(mitsuba-render PRIVATE $<TARGET_FILE:embree>)
  else()
      target_link_libraries(mitsuba-render PRIVATE embree)
  endif()
  add_dist(embree)
endif()

# Copy to 'dist' directory
add_dist(mitsuba-render)

# Python bindings
add_subdirectory(python)

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
