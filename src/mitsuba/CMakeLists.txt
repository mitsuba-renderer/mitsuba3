include_directories(
  ${ASMJIT_INCLUDE_DIRS}
)

add_executable(mitsuba mitsuba.cpp)

target_link_libraries(mitsuba PRIVATE mitsuba-core mitsuba-render)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  target_link_libraries(mitsuba PRIVATE asmjit)
endif()
