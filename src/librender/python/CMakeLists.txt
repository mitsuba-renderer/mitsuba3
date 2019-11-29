add_mitsuba_python_library(mitsuba_render_ext render
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
  sampler.cpp
  scene.cpp
  sensor.cpp
  shape.cpp
  texture.cpp
  spiral.cpp
  srgb.cpp
#   texture3d.cpp
)

target_link_libraries(mitsuba_render_ext PRIVATE mitsuba-core mitsuba-render tbb)
