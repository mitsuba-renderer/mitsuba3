add_mitsuba_python_library(mitsuba_ui_ext ui
  main.cpp
)

target_link_libraries(mitsuba_ui_ext PRIVATE mitsuba-core mitsuba-render mitsuba-ui)
