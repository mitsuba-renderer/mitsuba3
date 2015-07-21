add_executable(mitsuba mitsuba.cpp)
target_link_libraries(mitsuba mitsuba-core)

add_custom_command(
  TARGET mitsuba
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mitsuba> ${CMAKE_BINARY_DIR}/dist)