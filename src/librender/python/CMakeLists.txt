foreach (MTS_VARIANT ${MTS_VARIANTS})
  string(REPLACE "|" ";" MTS_VARIANT ${MTS_VARIANT})
  list(GET MTS_VARIANT 0 MTS_VARIANT_NAME)
  list(GET MTS_VARIANT 1 MTS_VARIANT_FLOAT)
  list(GET MTS_VARIANT 2 MTS_VARIANT_SPECTRUM)
  set(TARGET_NAME render_${MTS_VARIANT_NAME}_ext)

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
  )

  target_compile_definitions(${TARGET_NAME} PRIVATE
    "-DMTS_VARIANT_NAME=${MTS_VARIANT_NAME}"
    "-DMTS_VARIANT_FLOAT=${MTS_VARIANT_FLOAT}"
    "-DMTS_VARIANT_SPECTRUM=${MTS_VARIANT_SPECTRUM}"
  )

  target_link_libraries(${TARGET_NAME} PRIVATE mitsuba-render)

  set_target_properties(${TARGET_NAME}
    PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/python/mitsuba
    INSTALL_RPATH "${MTS_ORIGIN}/../.."
    FOLDER python
  )

  install(TARGETS ${TARGET_NAME}
    LIBRARY DESTINATION python/mitsuba
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
  spiral.cpp
  volumegrid.cpp
)

target_link_libraries(render_ext PRIVATE mitsuba-render)

set_target_properties(render_ext
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/python/mitsuba
  INSTALL_RPATH "${MTS_ORIGIN}/../.."
  FOLDER python
)

install(TARGETS render_ext
  LIBRARY DESTINATION python/mitsuba
)
