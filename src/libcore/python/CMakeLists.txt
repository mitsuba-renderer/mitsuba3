add_mitsuba_python_library(mitsuba_core_ext core
  # main
  mitsuba_core_ext.cpp

  # libcore
  appender.cpp
  atomic.cpp
  filesystem.cpp
  formatter.cpp
  logger.cpp
  math.cpp
  util.cpp
  object.cpp
  properties.cpp
  random.cpp
  stream.cpp
  thread.cpp
  argparser.cpp
  xml.cpp
  fresolver.cpp
  vector.cpp
  bbox.cpp
  frame.cpp
  transform.cpp
  ray.cpp
  struct.cpp
  bitmap.cpp
  spectrum.cpp
  warp.cpp
  qmc.cpp
  spline.cpp
  cast.cpp
  ddistr.cpp
  rfilter.cpp
)

target_link_libraries(mitsuba_core_ext PRIVATE mitsuba-core mitsuba-render tbb asmjit)
