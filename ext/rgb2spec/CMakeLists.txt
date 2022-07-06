cmake_minimum_required(VERSION 3.13...3.18)
project(rgb2spec)

# Set a default build configuration (Release)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

add_library(rgb2spec STATIC rgb2spec.c)
set_property(TARGET rgb2spec PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(rgb2spec_opt rgb2spec_opt.cpp)
set_target_properties(rgb2spec_opt PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO)

if (NOT APPLE)
  find_package(OpenMP)
  if(OpenMP_CXX_FOUND)
      target_link_libraries(rgb2spec_opt PRIVATE OpenMP::OpenMP_CXX)
  endif()
endif()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/srgb.coeff
  DEPENDS rgb2spec_opt
  COMMAND $<TARGET_FILE:rgb2spec_opt> 64 ${CMAKE_CURRENT_BINARY_DIR}/srgb.coeff
)

add_custom_target(
  rgb2spec_opt_run
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/srgb.coeff
)
