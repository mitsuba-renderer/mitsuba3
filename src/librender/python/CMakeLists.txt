add_mitsuba_python_library(mitsuba_render_ext render
  bsdf.cpp
  common.cpp
  emitter.cpp
  endpoint.cpp
  film.cpp
  imageblock.cpp
  integrator.cpp
  interaction.cpp
  main.cpp
  microfacet.cpp
  records.cpp
  rtbench.cpp
  sampler.cpp
  scene.cpp
  sensor.cpp
  shape.cpp
  spiral.cpp
)

target_link_libraries(mitsuba_render_ext PRIVATE mitsuba-core mitsuba-render tbb)
