set(INC_DIR "../../include/mitsuba/render")

set(LIBRENDER_EXTRA_SRC "")

if (MTS_ENABLE_OPTIX)
  # Glob up PTX files
  file(GLOB resources "${PROJECT_SOURCE_DIR}/resources/ptx/*.ptx")

  # Concatenate resource files into a comma separated string
  string (REGEX REPLACE "([^\\]|^);" "\\1," resources_string "${resources}")
  string (REGEX REPLACE "[\\](.)" "\\1" resources_string "${resources_string}")

  # Create command line for running bin2c cmake script
  set(bin2c_cmdline
    -DOUTPUT_C=librender_ptx.cpp
    -DOUTPUT_H=librender_ptx.h
    "-DINPUT_FILES=${resources_string}"
    -P "${PROJECT_SOURCE_DIR}/ext/nanogui/resources/bin2c.cmake")

  # Run bin2c on resource files
  add_custom_command(
    OUTPUT librender_ptx.cpp librender_ptx.h
    COMMAND ${CMAKE_COMMAND} ARGS ${bin2c_cmdline}
    DEPENDS ${resources}
    COMMENT "Running bin2c"
    PRE_BUILD VERBATIM)

  set(LIBRENDER_EXTRA_SRC librender_ptx.cpp librender_ptx.h)
  include_directories(${CMAKE_CURRENT_BINARY_DIR})
endif()

include_directories(
  ${RGB2SPEC_INCLUDE_DIRS}
)

add_library(mitsuba-render-obj OBJECT
  ${INC_DIR}/fwd.h
  ${INC_DIR}/ior.h
  ${INC_DIR}/microfacet.h
  ${INC_DIR}/records.h
  ${INC_DIR}/volume_texture.h

  bsdf.cpp         ${INC_DIR}/bsdf.h
  emitter.cpp      ${INC_DIR}/emitter.h
  endpoint.cpp     ${INC_DIR}/endpoint.h
  film.cpp         ${INC_DIR}/film.h
                   ${INC_DIR}/fresnel.h
  imageblock.cpp   ${INC_DIR}/imageblock.h
  integrator.cpp   ${INC_DIR}/integrator.h
                   ${INC_DIR}/interaction.h
  kdtree.cpp       ${INC_DIR}/kdtree.h
  medium.cpp       ${INC_DIR}/medium.h
  mesh.cpp         ${INC_DIR}/mesh.h
  microfacet.cpp   ${INC_DIR}/microfacet.h
                   ${INC_DIR}/mueller.h
  phase.cpp        ${INC_DIR}/phase.h
  sampler.cpp      ${INC_DIR}/sampler.h
  scene.cpp        ${INC_DIR}/scene.h
  sensor.cpp       ${INC_DIR}/sensor.h
  shape.cpp        ${INC_DIR}/shape.h
  shapegroup.cpp   ${INC_DIR}/shapegroup.h
  texture.cpp      ${INC_DIR}/texture.h
  spiral.cpp       ${INC_DIR}/spiral.h
  srgb.cpp         ${INC_DIR}/srgb.h
                   ${INC_DIR}/optix/common.h
  optix_api.cpp    ${INC_DIR}/optix_api.h
  ${LIBRENDER_EXTRA_SRC}
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
if (MTS_ENABLE_EMBREE)
  if (APPLE)
      target_link_libraries(mitsuba-render PRIVATE $<TARGET_FILE:embree>)
  else()
      target_link_libraries(mitsuba-render PRIVATE embree)
  endif()
  add_dist(embree)
endif()

if (MTS_ENABLE_OPTIX)
  target_link_libraries(mitsuba-render PRIVATE cuda)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "^(GNU)$")
  target_link_libraries(mitsuba-render PRIVATE -Wl,--no-undefined)
endif()

# Copy to 'dist' directory
add_dist(mitsuba-render)

# Python bindings
if (MTS_ENABLE_PYTHON)
  add_subdirectory(python)
endif()

# Register the test directory
add_tests(${CMAKE_CURRENT_SOURCE_DIR}/tests)
