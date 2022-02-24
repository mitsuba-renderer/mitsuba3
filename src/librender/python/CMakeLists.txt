foreach (MI_VARIANT ${MI_VARIANTS})
  string(REPLACE "|" ";" MI_VARIANT ${MI_VARIANT})
  list(GET MI_VARIANT 0 MI_VARIANT_NAME)
  list(GET MI_VARIANT 1 MI_VARIANT_FLOAT)
  list(GET MI_VARIANT 2 MI_VARIANT_SPECTRUM)
  set(TARGET_NAME render_${MI_VARIANT_NAME}_ext)

  pybind11_add_module(${TARGET_NAME}
    THIN_LTO OPT_SIZE
    main_v.cpp
    bsdf_v.cpp
    emitter_v.cpp
    endpoint_v.cpp
    film_v.cpp
    fresnel_v.cpp
    imageblock_v.cpp
    interaction_v.cpp
    integrator_v.cpp
    medium_v.cpp
    mueller_v.cpp
    microfacet_v.cpp
    phase_v.cpp
    records_v.cpp
    sampler_v.cpp
    scene_v.cpp
    sensor_v.cpp
    shape_v.cpp
    srgb_v.cpp
    texture_v.cpp
    volume_v.cpp
    volumegrid_v.cpp
    signal.h
  )

  target_compile_definitions(${TARGET_NAME} PRIVATE
    "-DMI_VARIANT_NAME=${MI_VARIANT_NAME}"
    "-DMI_VARIANT_FLOAT=${MI_VARIANT_FLOAT}"
    "-DMI_VARIANT_SPECTRUM=${MI_VARIANT_SPECTRUM}"
    # macOS: Avoid warnings about loop unrolling being disabled with -Os
    "-DDRJIT_UNROLL= "
  )

  target_link_libraries(${TARGET_NAME} PRIVATE mitsuba-render)

  set_target_properties(${TARGET_NAME}
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/python/mitsuba
    FOLDER python
  )

  install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION mitsuba
  )
endforeach()

pybind11_add_module(render_ext
  THIN_LTO OPT_SIZE
  emitter.cpp
  main.cpp
  bsdf.cpp
  interaction.cpp
  microfacet.cpp
  phase.cpp
  sensor.cpp
  spiral.cpp
  film.cpp
)

# macOS: Avoid warnings about loop unrolling being disabled with -Os
target_compile_definitions(render_ext PRIVATE "-DDRJIT_UNROLL= ")

target_link_libraries(render_ext PRIVATE mitsuba-render)

set_target_properties(render_ext
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/python/mitsuba
  FOLDER python
)

install(TARGETS render_ext
  LIBRARY DESTINATION mitsuba
)
