include_directories(
  ${NANOGUI_INCLUDE_DIRS}
  ${PCG32_INCLUDE_DIRS}
)

add_definitions(
  -DMTS_BUILD_MODULE=MTS_MODULE_UI
  ${NANOGUI_EXTRA_DEFS}
)

set(INC_DIR "../../include/mitsuba/ui")

add_library(mitsuba-ui SHARED
  gltexture.cpp     ${INC_DIR}/gltexture.h
)

# Link to Mitsuba core library
target_link_libraries(mitsuba-ui mitsuba-core)

# Link to NanoGUI
target_link_libraries(mitsuba-ui nanogui)
set_target_properties(mitsuba-ui PROPERTIES FOLDER mitsuba-ui)

# Copy to 'dist' directory
add_dist(mitsuba-ui nanogui)
