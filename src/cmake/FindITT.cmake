# Copyright (c) 2017-2018 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# VTune is a source of ITT library
if (NOT CMAKE_VTUNE_HOME)
  if (APPLE)
    # TODO: find it generically
    set(CMAKE_VTUNE_HOME "/Applications/Intel VTune Amplifier 2019.app/Contents")
  elseif (UNIX)
    set(CMAKE_VTUNE_HOME /opt/intel/vtune_amplifier)
  endif()
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( arch "64" )
else()
  set( arch "32" )
endif()

find_path( ITT_INCLUDE_DIRS ittnotify.h
  PATHS ${CMAKE_ITT_HOME} ${CMAKE_VTUNE_HOME}
  PATH_SUFFIXES include )
message(STATUS "VTune home: ${CMAKE_VTUNE_HOME}")
message(STATUS "Include dirs: ${ITT_INCLUDE_DIRS}")

# Unfortunately SEAPI and VTune uses different names for itt library:
#  * SEAPI uses libittnotify${arch}.a
#  * VTune uses libittnotify.a
# We are trying to check both giving preference to SEAPI name.
find_path( ITT_LIBRARY_DIRS libittnotify${arch}.a
  PATHS ${CMAKE_ITT_HOME} ${CMAKE_VTUNE_HOME}
  PATH_SUFFIXES lib64 )
if( NOT ITT_LIBRARY_DIRS MATCHES NOTFOUND )
  set( ITT_LIBRARIES "ittnotify${arch}" )
else()
  find_path( ITT_LIBRARY_DIRS libittnotify.a
    PATHS ${CMAKE_ITT_HOME} ${CMAKE_VTUNE_HOME}
    PATH_SUFFIXES lib64 )
  if( NOT ITT_LIBRARY_PATH MATCHES NOTFOUND )
    set( ITT_LIBRARIES "ittnotify" )
  endif()
endif()

if(NOT ITT_INCLUDE_DIRS MATCHES NOTFOUND AND
   NOT ITT_LIBRARY_DIRS MATCHES NOTFOUND)

  message( STATUS "itt header is in ${ITT_INCLUDE_DIRS}" )
  message( STATUS "itt lib is in ${ITT_LIBRARY_DIRS}" )
  message( STATUS "itt lib name is ${ITT_LIBRARIES}" )

  include_directories( ${ITT_INCLUDE_DIRS} )
  link_directories( ${ITT_LIBRARY_DIRS} )

  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMFX_TRACE_ENABLE_ITT" )
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMFX_TRACE_ENABLE_ITT" )

  set( ITT_FOUND TRUE )
endif()

if (NOT ITT_FOUND)
  message( FATAL_ERROR "Failed to find ITT library" )
endif()
