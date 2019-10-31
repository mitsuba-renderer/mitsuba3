add_mitsuba_python_library(mitsuba_core_ext core
  # main.cpp

  # libcore
  # appender.cpp
  # argparser.cpp
  # atomic.cpp
  # bbox.cpp
  # bitmap.cpp
  # bsphere.cpp
  # cast.cpp
  # ddistr.cpp
  # autodiff.cpp
  # filesystem.cpp
  # formatter.cpp
  # frame.cpp
  # fresolver.cpp
  # logger.cpp
  # math.cpp
  # mmap.cpp
  # object.cpp
  # progress.cpp
  # properties.cpp
  # qmc.cpp
  # quad.cpp
  # random.cpp
  # ray.cpp
  # rfilter.cpp
  # spectrum.cpp
  # spline.cpp
  # stream.cpp
  # struct.cpp
  # thread.cpp
  # transform.cpp
  # util.cpp
  # vector.cpp
  warp.cpp
  # xml.cpp
)

target_link_libraries(mitsuba_core_ext PRIVATE mitsuba-core mitsuba-render tbb)

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
  target_link_libraries(mitsuba_core_ext PRIVATE asmjit)
endif()
