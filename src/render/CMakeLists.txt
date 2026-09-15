set(INC_DIR "../../include/mitsuba/render")

set(LIBRENDER_EXTRA_SRC "")

include_directories(
  ${RGB2SPEC_INCLUDE_DIRS}
)

if (NOT MI_ENABLE_EMBREE)
  set(LIBRENDER_EXTRA_SRC kdtree.cpp ${INC_DIR}/kdtree.h ${LIBRENDER_EXTRA_SRC})
endif()

if (MI_ENABLE_CUDA)
  set(LIBRENDER_EXTRA_SRC
    optixdenoiser.cpp ${INC_DIR}/optixdenoiser.h
    optix/accel.cpp optix/accel.h optix/common.h
    ${LIBRENDER_EXTRA_SRC}
  )
endif()

if (MI_ENABLE_METAL)
  set(LIBRENDER_EXTRA_SRC
    metal_accel.mm metal/accel.h
    ${LIBRENDER_EXTRA_SRC}
  )
  set_source_files_properties(metal_accel.mm
    PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

add_library(mitsuba-render OBJECT
  ${INC_DIR}/fwd.h
  ${INC_DIR}/ior.h
  ${INC_DIR}/microfacet.h
  ${INC_DIR}/records.h

  bsdf.cpp         ${INC_DIR}/bsdf.h
  dedge.cpp ${INC_DIR}/dedge.h
  emitter.cpp      ${INC_DIR}/emitter.h
  endpoint.cpp     ${INC_DIR}/endpoint.h
  film.cpp         ${INC_DIR}/film.h
                   ${INC_DIR}/fresnel.h
  imageblock.cpp   ${INC_DIR}/imageblock.h
  integrator.cpp   ${INC_DIR}/integrator.h
                   ${INC_DIR}/interaction.h
  medium.cpp       ${INC_DIR}/medium.h
  mesh.cpp         ${INC_DIR}/mesh.h
  mesh_utils.cpp   ${INC_DIR}/mesh_utils.h
  microfacet.cpp   ${INC_DIR}/microfacet.h
                   ${INC_DIR}/mueller.h
  phase.cpp        ${INC_DIR}/phase.h
  sampler.cpp      ${INC_DIR}/sampler.h
  scene.cpp        ${INC_DIR}/scene.h
  scene_ir.cpp     ${INC_DIR}/scene_ir.h
  sensor.cpp       ${INC_DIR}/sensor.h
  shape.cpp        ${INC_DIR}/shape.h
  texture.cpp      ${INC_DIR}/texture.h
                   ${INC_DIR}/microflake.h
  spiral.cpp       ${INC_DIR}/spiral.h
  srgb.cpp         ${INC_DIR}/srgb.h
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
target_link_libraries(mitsuba-render PUBLIC struct-jit)

if (MI_ENABLE_METAL)
  target_link_libraries(mitsuba-render PRIVATE "-framework Metal" "-framework Foundation")
endif()

if (MI_ENABLE_JIT)
  target_link_libraries(mitsuba-render PUBLIC drjit-core)
endif()

# Python bindings
if (MI_ENABLE_PYTHON)
  add_subdirectory(python)
  set(RENDER_PY_V_SRC ${RENDER_PY_V_SRC} PARENT_SCOPE)
  set(RENDER_PY_SRC   ${RENDER_PY_SRC}   PARENT_SCOPE)
endif()
