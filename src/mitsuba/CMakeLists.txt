include_directories(
  ${TBB_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
)

add_executable(mitsuba mitsuba.cpp)

target_link_libraries(mitsuba mitsuba-core mitsuba-render tbb)

add_custom_command(
  TARGET mitsuba
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:mitsuba> ${CMAKE_BINARY_DIR}/dist)

if (APPLE)
  set_target_properties(mitsuba PROPERTIES INSTALL_RPATH "@executable_path")
endif()
