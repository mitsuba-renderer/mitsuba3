cmake_minimum_required (VERSION 3.2.0)
project(rgb2spec)

if (POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
endif()

# Set a default build configuration (Release)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()


if (TARGET tbb)
  add_definitions(-DRGB2SPEC_USE_TBB=1)
  include_directories(${TBB_INCLUDE_DIRS})
else ()
  include(FindOpenMP)
  if(OPENMP_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
  endif()
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_executable(rgb2spec_opt rgb2spec_opt.cpp)
add_library(rgb2spec STATIC rgb2spec.c)

if (TARGET tbb)
  target_link_libraries(rgb2spec_opt PRIVATE tbb)

  add_custom_command(TARGET rgb2spec_opt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:tbb>
      $<TARGET_FILE_DIR:rgb2spec_opt>
  )
endif()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/srgb.coeff
  DEPENDS rgb2spec_opt
  COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=.:$ENV{LD_LIBRARY_PATH}
  $<TARGET_FILE:rgb2spec_opt> 64 ${CMAKE_CURRENT_BINARY_DIR}/srgb.coeff
)

add_custom_target(
  rgb2spec_opt_run
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/srgb.coeff
)
