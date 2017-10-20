add_mitsuba_python_library(mitsuba_render_ext render
  bsdf.cpp
  common.cpp
  emitter.cpp
  endpoint.cpp
  interaction.cpp
  main.cpp
  records.cpp
  rt.cpp
  scene.cpp
  sensor.cpp
  shape.cpp
)

target_link_libraries(mitsuba_render_ext PRIVATE mitsuba-core mitsuba-render tbb)
