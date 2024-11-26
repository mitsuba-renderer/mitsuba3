#include <mitsuba/render/shapegroup.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-shapegroup:

Shape group (:monosp:`shapegroup`)
----------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - :paramtype:`shape`
   - One or more shapes that should be made available for geometry instancing

This plugin implements a container for shapes that should be made available for geometry instancing.
Any shapes placed in a shapegroup will not be visible on their own—instead, the renderer will
precompute ray intersection acceleration data structures so that they can efficiently be referenced
many times using the :ref:`shape-instance` plugin. This is useful for rendering things like forests,
where only a few distinct types of trees have to be kept in memory. An example is given below:

.. tabs::
    .. code-tab:: xml
        :name: shapegroup

        <!-- Declare a named shape group containing two objects -->
        <shape type="shapegroup" id="my_shape_group">
            <shape type="ply">
                <string name="filename" value="data.ply"/>
                <bsdf type="roughconductor"/>
            </shape>
            <shape type="sphere">
                <transform name="to_world">
                    <translate y="20"/>
                    <scale value="5"/>
                </transform>
                <bsdf type="diffuse"/>
            </shape>
        </shape>

        <!-- Instantiate the shape group without any kind of transformation -->
        <shape type="instance">
            <ref id="my_shape_group"/>
        </shape>

        <!-- Create instance of the shape group, but rotated, scaled, and translated -->
        <shape type="instance">
            <ref id="my_shape_group"/>
            <transform name="to_world">
                <translate z="10"/>
                <scale value="1.5"/>
                <rotate x="1" angle="45"/>
            </transform>
        </shape>

    .. code-tab:: python

        # Declare a named shape group containing two objects
        'my_shape_group': {
            'type': 'shapegroup',
            'first_object': {
                'type': 'ply',
                'bsdf': {
                    'type': 'roughconductor',
                }
            },
            'second_object': {
                'type': 'sphere',
                'to_world': mi.ScalarTransform4f().scale([5, 5, 5]).translate([0, 20, 0])
                'bsdf': {
                    'type': 'diffuse',
                }
            }
        },

        # Instantiate the shape group without any kind of transformation
        'first_instance': {
            'type': 'instance',
            'shapegroup': {
                'type': 'ref',
                'id': 'my_shape_group'
            }
        },

        # Create instance of the shape group, but rotated, scaled, and translated
        'second_instance': {
            'type': 'instance',
            'to_world': mi.ScalarTransform4f().rotate([1, 0, 0], 45).scale([1.5, 1.5, 1.5]).translate([0, 10, 0]),
            'shapegroup': {
                'type': 'ref',
                'id': 'my_shape_group'
            }
        }

 */

template <typename Float, typename Spectrum>
class ShapeGroupPlugin final : public ShapeGroup<Float, Spectrum> {
public:
    MI_IMPORT_BASE(ShapeGroup)
    MI_IMPORT_TYPES()

    ShapeGroupPlugin(const Properties &props) : Base(props) { }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(ShapeGroupPlugin, ShapeGroup)
MI_EXPORT_PLUGIN(ShapeGroupPlugin, "Shape group plugin")

NAMESPACE_END(mitsuba)
