#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-portal:

Light portal (:monosp:`portal`)
-------------------------------------------------

.. pluginparameters::

 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. (Default: none (i.e.
object space = world space))

A light portal marks an opening, such as a window or a door, through which
the environment illuminates an otherwise enclosed part of the scene. Like the
:ref:`rectangle <shape-rectangle>` shape, it covers the XY-range
:math:`[-1,1]\times[-1,1]` in its local frame and its normal points along the
positive Z axis. The normal must point away from the environment and into the
region that receives its light. :monosp:`to_world` may translate, rotate and
scale the square but must not shear it.

While they are declared as emitters, portals do not emit any light and are
never sampled or intersected on their own. Their only effect is that the
:ref:`constant <emitter-constant>` and :ref:`envmap <emitter-envmap>` emitters
sample directions through the portals facing the shading point with the
probability given by the scene's :monosp:`portal_weight` parameter. This
reduces noise in enclosed spaces and leaves the expected image unchanged.

.. tabs::
    .. code-tab:: xml
        :name: portal

        <emitter type="portal">
            <transform name="to_world">
                <scale x="1.5" y="1"/>
                <rotate y="1" angle="90"/>
                <translate x="-4" y="1.5" z="0"/>
            </transform>
        </emitter>

    .. code-tab:: python

        'type': 'portal',
        'to_world': mi.ScalarAffineTransform4f().translate([-4, 1.5, 0]) \\
                                                .rotate([0, 1, 0], 90) \\
                                                .scale([1.5, 1, 1])

 */

template <typename Float, typename Spectrum>
class Portal final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MI_IMPORT_TYPES(Shape)

    Portal(const Properties &props) : Base(props) {
        // Solid angle sampling needs a rectangle
        ScalarVector3f du = m_to_world.scalar() * ScalarVector3f(1.f, 0.f, 0.f),
                       dv = m_to_world.scalar() * ScalarVector3f(0.f, 1.f, 0.f);
        ScalarFloat len_u = dr::norm(du), len_v = dr::norm(dv);
        if (!(len_u > 0.f && len_v > 0.f))
            Throw("Portal with transform %s is degenerate!", m_to_world.scalar());
        if (dr::abs(dr::dot(du, dv)) > 1e-4f * len_u * len_v)
            Throw("Portals must be rectangular: 'to_world' must map the local "
                  "x and y axes to orthogonal vectors (got %s)!", m_to_world.scalar());

        m_flags = +EmitterFlags::Portal;

        // Portals are never reached by traced calls
        this->unregister();
    }

    void set_shape(Shape *) override {
        Throw("Portals cannot be attached to shapes!");
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        for (ScalarFloat x : { -1.f, 1.f })
            for (ScalarFloat y : { -1.f, 1.f })
                bbox.expand(m_to_world.scalar() * ScalarPoint3f(x, y, 0.f));
        return bbox;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Portal[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(Portal)
};

MI_EXPORT_PLUGIN(Portal)
NAMESPACE_END(mitsuba)
