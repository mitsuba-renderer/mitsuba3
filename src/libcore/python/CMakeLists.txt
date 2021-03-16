foreach (MTS_VARIANT ${MTS_VARIANTS})
  string(REPLACE "|" ";" MTS_VARIANT ${MTS_VARIANT})
  list(GET MTS_VARIANT 0 MTS_VARIANT_NAME)
  list(GET MTS_VARIANT 1 MTS_VARIANT_FLOAT)
  list(GET MTS_VARIANT 2 MTS_VARIANT_SPECTRUM)
  set(TARGET_NAME core_${MTS_VARIANT_NAME}_ext)

  pybind11_add_module(${TARGET_NAME}
    THIN_LTO OPT_SIZE
    main_v.cpp
    enoki_v.cpp
    bbox_v.cpp
    bsphere_v.cpp
    distr_1d_v.cpp
    distr_2d_v.cpp
    frame_v.cpp
    math_v.cpp
    object_v.cpp
    qmc_v.cpp
    properties_v.cpp
    random_v.cpp
    ray_v.cpp
    rfilter_v.cpp
    spectrum_v.cpp
    spline_v.cpp
    transform_v.cpp
    vector_v.cpp
    warp_v.cpp
    xml_v.cpp
    quad_v.cpp
  )

  target_compile_definitions(${TARGET_NAME} PRIVATE
    "-DMTS_VARIANT_NAME=${MTS_VARIANT_NAME}"
    "-DMTS_VARIANT_FLOAT=${MTS_VARIANT_FLOAT}"
    "-DMTS_VARIANT_SPECTRUM=${MTS_VARIANT_SPECTRUM}"
  )

  target_link_libraries(${TARGET_NAME} PRIVATE mitsuba-core)

  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
    target_link_libraries(${TARGET_NAME} PRIVATE asmjit)
  endif()

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

pybind11_add_module(core_ext
  THIN_LTO OPT_SIZE
  main.cpp
  atomic.cpp
  appender.cpp
  argparser.cpp
  bitmap.cpp
  cast.cpp
  filesystem.cpp
  formatter.cpp
  fresolver.cpp
  logger.cpp
  mmap.cpp
  object.cpp
  progress.cpp
  rfilter.cpp
  stream.cpp
  struct.cpp
  thread.cpp
  util.cpp
)

target_link_libraries(core_ext PRIVATE mitsuba-core mitsuba-render asmjit)
set_target_properties(core_ext PROPERTIES INSTALL_RPATH "${MTS_ORIGIN}/../..")

set_target_properties(core_ext
  PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${MTS_BINARY_DIR}/python/mitsuba
  INSTALL_RPATH "${MTS_ORIGIN}/../.."
  FOLDER python
)

install(TARGETS core_ext
  LIBRARY DESTINATION python/mitsuba
)
