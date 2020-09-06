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
# set(ENOKI_PYTHON_FILES
#    __init__.py
#    const.py
#    detail.py
#    generic.py
#    matrix.py
#    router.py
#    traits.py
# )
#
# set(ENOKI_PYTHON_OUTPUT "")
# foreach(file ${ENOKI_PYTHON_FILES})
#     add_custom_command(
#         OUTPUT ${CMAKE_BINARY_DIR}/dist/python/enoki/${file}
#         COMMAND ${CMAKE_COMMAND} -E copy
#         ${PROJECT_SOURCE_DIR}/ext/enoki/enoki/${file}
#         ${CMAKE_BINARY_DIR}/dist/python/enoki/${file}
#     )
#     set(ENOKI_PYTHON_OUTPUT ${ENOKI_PYTHON_OUTPUT} ${CMAKE_BINARY_DIR}/dist/python/enoki/${file})
# endforeach(file)
#
# add_custom_target(
#   mitsuba-enoki-python-init
#   ALL DEPENDS ${ENOKI_PYTHON_OUTPUT}
# )

# docstring generation support
# include("cmake/docstrings.cmake")
