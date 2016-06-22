include_directories(
  ${NANOGUI_INCLUDE_DIRS}
)

add_definitions(
  -DMTS_BUILD_MODULE=MTS_MODULE_GUI
)

set(INC_DIR "../../include/mitsuba/gui")

add_library(mitsuba-gui SHARED
  warp_visualizer.cpp     ${INC_DIR}/warp_visualizer.h
)

# Link to Mitsuba core library
target_link_libraries(mitsuba-gui mitsuba-core)
# Link to NanoGUI
target_link_libraries(mitsuba-gui nanogui)

set(DEPENDENCIES mitsuba-gui nanogui)

# Copy dependencies into the 'dist' directory
foreach(dep ${DEPENDENCIES})
  add_custom_command(
    TARGET mitsuba-gui
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${dep}> ${CMAKE_BINARY_DIR}/dist)
endforeach(dep)
