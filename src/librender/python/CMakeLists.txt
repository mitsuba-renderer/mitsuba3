add_mitsuba_python_library(mitsuba_render_ext render
  main.cpp
  common.cpp
  interaction.cpp
  records.cpp
  scene.cpp
  shape.cpp
)

target_link_libraries(mitsuba_render_ext PRIVATE mitsuba-core mitsuba-render tbb)
