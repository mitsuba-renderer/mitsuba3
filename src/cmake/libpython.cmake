#
# if (MTS_ENABLE_GUI)
#     # Copy nanogui python library to 'dist' folder
#     # add_dist(python/nanogui-python)
#     if (APPLE)
#     set_target_properties(nanogui-python PROPERTIES INSTALL_RPATH "@loader_path/..")
#     elseif(UNIX)
#     set_target_properties(nanogui-python PROPERTIES INSTALL_RPATH "$ORIGIN/..")
#     endif()
#     set_target_properties(nanogui-python PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
# endif()
#
# # Copy enoki python library to 'dist' folder
# # add_dist(python/enoki/enoki-python)
# if (APPLE)
#     set_target_properties(enoki-python PROPERTIES INSTALL_RPATH "@loader_path/../..")
# elseif(UNIX)
#     set_target_properties(enoki-python PROPERTIES INSTALL_RPATH "$ORIGIN/../..")
# endif()
# set_target_properties(enoki-python PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
#

# docstring generation support
# include("cmake/docstrings.cmake")
