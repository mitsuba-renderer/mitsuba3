add_mitsuba_python_library(mitsuba_render_ext render
  autodiff.cpp
  bsdf.cpp
  emitter.cpp
  endpoint.cpp
  film.cpp
  fresnel.cpp
  imageblock.cpp
  integrator.cpp
  interaction.cpp
  main.cpp
  microfacet.cpp
  mueller.cpp
  records.cpp
  rtbench.cpp
  sampler.cpp
  scene.cpp
  sensor.cpp
  shape.cpp
  spectrum.cpp
  spiral.cpp
  srgb.cpp
  texture3d.cpp
)

target_link_libraries(mitsuba_render_ext PRIVATE mitsuba-core mitsuba-render tbb)
