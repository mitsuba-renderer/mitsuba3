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
  )

  target_compile_definitions(${TARGET_NAME}-obj PRIVATE
    "-DMTS_VARIANT_NAME=${MTS_VARIANT_NAME}"
    "-DMTS_VARIANT_FLOAT=${MTS_VARIANT_FLOAT}"
    "-DMTS_VARIANT_SPECTRUM=${MTS_VARIANT_SPECTRUM}"
    "-DMTS_VARIANT_VECTORIZE=${MTS_VARIANT_VECTORIZE}"
  )

  target_link_libraries(${TARGET_NAME} PRIVATE mitsuba-core mitsuba-render tbb)

  if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
    target_link_libraries(${TARGET_NAME} PRIVATE asmjit)
  endif()
endforeach()

add_mitsuba_python_library(core_ext
  main.cpp
)

#add_mitsuba_python_library(mitsuba_core_ext core
  #main.cpp

  ## libcore
  #appender.cpp
  #argparser.cpp
  #atomic.cpp
  #bbox.cpp
  #bitmap.cpp
  #bsphere.cpp
  #cast.cpp
  #ddistr.cpp
  #filesystem.cpp
  #formatter.cpp
  #frame.cpp
  #fresolver.cpp
  #logger.cpp
  #math.cpp
  #mmap.cpp
  #object.cpp
  #progress.cpp
  #properties.cpp
  #qmc.cpp
  #quad.cpp
  #random.cpp
  #ray.cpp
  #rfilter.cpp
  #spectrum.cpp
  #spline.cpp
  #stream.cpp
  #struct.cpp
  #thread.cpp
  #transform.cpp
  #util.cpp
  #vector.cpp
  #warp.cpp
  #xml.cpp
#)

