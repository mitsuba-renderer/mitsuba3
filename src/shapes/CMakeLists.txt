set(MI_PLUGIN_PREFIX "shapes")

add_plugin(obj          obj.cpp)
add_plugin(ply          ply.cpp)
add_plugin(blender      blender.cpp)
add_plugin(serialized   serialized.cpp)

add_plugin(cylinder     cylinder.cpp)
add_plugin(disk         disk.cpp)
add_plugin(rectangle    rectangle.cpp)
add_plugin(sdfgrid      sdfgrid.cpp)
add_plugin(sphere       sphere.cpp)
add_plugin(cube         cube.cpp)
add_plugin(bsplinecurve bsplinecurve.cpp)
add_plugin(linearcurve  linearcurve.cpp)

add_plugin(shapegroup   shapegroup.cpp)
add_plugin(instance     instance.cpp)
add_plugin(merge        merge.cpp)

if (MI_ENABLE_EMBREE)
    target_link_libraries(sphere   PRIVATE embree)
    target_link_libraries(instance PRIVATE embree)
endif()

set(MI_PLUGIN_TARGETS "${MI_PLUGIN_TARGETS}" PARENT_SCOPE)
