# Find the NVIDIA DLSS SDK. This module defines
#  DLSS_INCLUDE_DIR, where to find the NGX headers for DLSS
#  DLSS_FOUND, if the DLSS SDK was found.
#
# Only the headers of the SDK are needed, the implementation is loaded from the
# NVIDIA display driver at runtime. Point ``DLSS_SDK_ROOT`` (either as a CMake
# variable or an environment variable) at a checkout of
# https://github.com/NVIDIA/DLSS to enable DLSS support.

find_path(DLSS_INCLUDE_DIR
  NAMES
    "nvsdk_ngx.h"
  PATHS
    "${DLSS_SDK_ROOT}/include"
    "$ENV{DLSS_SDK_ROOT}/include"
)

# The Ray Reconstruction implementation library. It is loaded by the NGX
# component of the driver, which looks for it next to the running executable, so
# the build copies it next to the Mitsuba binaries when it is found here.
if (WIN32)
  find_file(DLSS_RR_LIBRARY
    NAMES
      "nvngx_dlssd.dll"
    PATHS
      "${DLSS_SDK_ROOT}/lib/Windows_x86_64/rel"
      "$ENV{DLSS_SDK_ROOT}/lib/Windows_x86_64/rel"
  )
else()
  file(GLOB DLSS_RR_LIBRARY_CANDIDATES
    "${DLSS_SDK_ROOT}/lib/Linux_x86_64/rel/libnvidia-ngx-dlssd.so*"
    "$ENV{DLSS_SDK_ROOT}/lib/Linux_x86_64/rel/libnvidia-ngx-dlssd.so*"
  )
  if (DLSS_RR_LIBRARY_CANDIDATES)
    list(GET DLSS_RR_LIBRARY_CANDIDATES 0 DLSS_RR_LIBRARY)
  endif()
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(DLSS REQUIRED_VARS DLSS_INCLUDE_DIR)

mark_as_advanced(DLSS_INCLUDE_DIR DLSS_RR_LIBRARY)
