include_directories(
  ${TBB_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
)

add_definitions(-DMTS_BRANCH="${GIT_BRANCH}")
add_definitions(-DMTS_HASH="${GIT_COMMIT_HASH}")

add_executable(mitsuba mitsuba.cpp)

target_link_libraries(mitsuba mitsuba-core mitsuba-render tbb asmjit)

add_dist(mitsuba)

if (APPLE)
  set_target_properties(mitsuba PROPERTIES INSTALL_RPATH "@executable_path")
endif()
