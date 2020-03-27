if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel"
    AND NOT U_CMAKE_BUILD_TYPE MATCHES REL)
  # Default symbol visibility = hidden even in debug mode
  # (Otherwise module-local pybind11 exports don't work)
  add_compile_options(${TARGET_NAME} -fvisibility=hidden)
endif()

foreach (MTS_VARIANT ${MTS_VARIANTS})
  string(REPLACE "|" ";" MTS_VARIANT ${MTS_VARIANT})
  list(GET MTS_VARIANT 0 MTS_VARIANT_NAME)
  list(GET MTS_VARIANT 1 MTS_VARIANT_FLOAT)
  list(GET MTS_VARIANT 2 MTS_VARIANT_SPECTRUM)
  set(TARGET_NAME core_${MTS_VARIANT_NAME}_ext)

  if (MTS_VARIANT_FLOAT MATCHES "Packet")
    set(MTS_VARIANT_VECTORIZE 1)
  else()
    set(MTS_VARIANT_VECTORIZE 0)
  endif()

  add_mitsuba_python_library(${TARGET_NAME}
    main_v.cpp
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
  )

  target_compile_definitions(${TARGET_NAME}-obj PRIVATE
    "-DMTS_VARIANT_NAME=${MTS_VARIANT_NAME}"
    "-DMTS_VARIANT_FLOAT=${MTS_VARIANT_FLOAT}"
    "-DMTS_VARIANT_SPECTRUM=${MTS_VARIANT_SPECTRUM}"
    "-DMTS_VARIANT_VECTORIZE=${MTS_VARIANT_VECTORIZE}"
  )

  target_link_libraries(${TARGET_NAME} PRIVATE mitsuba-core tbb)

  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
    target_link_libraries(${TARGET_NAME} PRIVATE asmjit)
  endif()
endforeach()

add_mitsuba_python_library(core_ext
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
#   properties.cpp
  quad.cpp
  rfilter.cpp
  stream.cpp
  struct.cpp
  thread.cpp
  util.cpp
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel")
  target_compile_options(core_ext PRIVATE -fvisibility=hidden)
endif()

target_link_libraries(core_ext PRIVATE mitsuba-core mitsuba-render tbb asmjit)
