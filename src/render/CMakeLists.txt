set(INC_DIR "../../include/mitsuba/render")

set(LIBRENDER_EXTRA_SRC "")

if (MI_ENABLE_CUDA)
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

if (NOT MI_ENABLE_EMBREE)
  set(LIBRENDER_EXTRA_SRC kdtree.cpp ${INC_DIR}/kdtree.h ${LIBRENDER_EXTRA_SRC})
endif()

if (MI_ENABLE_CUDA)
  set(LIBRENDER_EXTRA_SRC
    optixdenoiser.cpp ${INC_DIR}/optixdenoiser.h
    ${LIBRENDER_EXTRA_SRC}
  )
endif()

add_library(mitsuba-render OBJECT
  ${INC_DIR}/fwd.h
  ${INC_DIR}/ior.h
  ${INC_DIR}/microfacet.h
  ${INC_DIR}/records.h

  bsdf.cpp         ${INC_DIR}/bsdf.h
  emitter.cpp      ${INC_DIR}/emitter.h
  endpoint.cpp     ${INC_DIR}/endpoint.h
  film.cpp         ${INC_DIR}/film.h
                   ${INC_DIR}/fresnel.h
  imageblock.cpp   ${INC_DIR}/imageblock.h
  integrator.cpp   ${INC_DIR}/integrator.h
                   ${INC_DIR}/interaction.h
  medium.cpp       ${INC_DIR}/medium.h
  mesh.cpp         ${INC_DIR}/mesh.h
  microfacet.cpp   ${INC_DIR}/microfacet.h
                   ${INC_DIR}/mueller.h
  phase.cpp        ${INC_DIR}/phase.h
  sampler.cpp      ${INC_DIR}/sampler.h
  scene.cpp        ${INC_DIR}/scene.h
  sensor.cpp       ${INC_DIR}/sensor.h
  shape.cpp        ${INC_DIR}/shape.h
  texture.cpp      ${INC_DIR}/texture.h
                   ${INC_DIR}/microflake.h
  spiral.cpp       ${INC_DIR}/spiral.h
  srgb.cpp         ${INC_DIR}/srgb.h
                   ${INC_DIR}/optix/common.h
  optix_api.cpp    ${INC_DIR}/optix_api.h
  shapegroup.cpp   ${INC_DIR}/shapegroup.h
  volume.cpp       ${INC_DIR}/volume.h
  volumegrid.cpp   ${INC_DIR}/volumegrid.h
  ${LIBRENDER_EXTRA_SRC}
)

target_compile_definitions(mitsuba-render
  PRIVATE -DMI_BUILD_MODULE=MI_MODULE_LIB)

set_target_properties(mitsuba-render PROPERTIES
  INTERPROCEDURAL_OPTIMIZATION ON)

set_target_properties(mitsuba-render PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

# Link to nanothread and rgb2spec
target_link_libraries(mitsuba-render PRIVATE nanothread rgb2spec)

# Link to Embree
if (MI_ENABLE_EMBREE)
    target_link_libraries(mitsuba-render PRIVATE embree)
endif()

target_link_libraries(mitsuba-render PUBLIC drjit)

if (MI_ENABLE_JIT)
  target_link_libraries(mitsuba-render PUBLIC drjit-core)
endif()

# Python bindings
if (MI_ENABLE_PYTHON)
  add_subdirectory(python)
  set(RENDER_PY_V_SRC ${RENDER_PY_V_SRC} PARENT_SCOPE)
  set(RENDER_PY_SRC   ${RENDER_PY_SRC}   PARENT_SCOPE)
endif()
