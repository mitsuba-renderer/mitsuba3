include_directories(
  ${TBB_INCLUDE_DIRS}
  ${ASMJIT_INCLUDE_DIRS}
  ${NANOGUI_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(
  ${NANOGUI_EXTRA_DEFS}
)

add_definitions(-DMTS_BRANCH="${GIT_BRANCH}")
add_definitions(-DMTS_HASH="${GIT_COMMIT_HASH}")

add_executable(mtsgui mtsgui.cpp)
target_link_libraries(mtsgui PRIVATE mitsuba-core mitsuba-render mitsuba-ui tbb)

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
  target_link_libraries(mtsgui PRIVATE asmjit)
endif()

add_dist(mtsgui)

if (APPLE)
  set_target_properties(mtsgui PROPERTIES INSTALL_RPATH "@executable_path")
endif()
