#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

enum class FilterType { Nearest, Trilinear };
enum class WrapMode { Repeat, Mirror, Clamp };

/**!
.. _plugin-volume-sphericalcoordsvolume:

Mapping to spherical coordinates (:monosp:`sphericalcoordsvolume`)
------------------------------------------------------------------

.. pluginparameters::

 * - volume
   - |volume|
   - Nested volume plugin whose data is to be mapped to spherical coordinates.
   - —

 * - rmin
   - |float|
   - Radius for the inner limit of the spherical shell, relative to the unit
     sphere. Default: 0
   - —

 * - rmax
   - |float|
   - Radius for the outer limit of the spherical shell, relative to the unit
     sphere. Default: 1
   - —

 * - fillmin
   - |float|
   - Constant value to return for points such that :math:`r < r_\mathrm{min}`.
     Default: 0
   - —

 * - fillmax
   - |float|
   - Constant value to return for points such that :math:`r_\mathrm{max} < r`.
     Default: 0
   - —

 * - to_world
   - |transform|
   - Specifies an optional 4x4 transformation matrix that will remap local
     spherical coordinates from the unit sphere (which covers the [-1, 1]³ cube)
     to world coordinates.
   - —

This plugin addresses volume data in spherical coordinates. In practice, it
maps the texture coordinates of a nested volume plugin to the unit sphere using
the following correspondance:

.. math::

    x \in [0, 1] & \leftrightarrow r \in [r_\mathrm{min}, r_\mathrm{max}] \\
    y \in [0, 1] & \leftrightarrow \theta \in [0, \pi] \\
    z \in [0, 1] & \leftrightarrow \phi \in [-\pi, \pi]

where :math:`r` is the radius of the considered point in the unit sphere.
For angles, the default mathematical convention is used:
:math:`\theta` is the zenith angle with respect to the :math:`+Z` unit vector,
and :math:`\phi` is the azimuth angle with respect to the :math:`(+X, +Z)`
plane.

.. note::

    When using this plugin with a nested ``gridvolume``, the data layout
    remains unchanged (*i.e.* zyxc will be interpreted as φθrc).
*/

// Note: This plugin is primarily designed to be used with a spherical stencil,
// but it should work with other shapes.

template <typename Float, typename Spectrum>
class SphericalCoordsVolume final : public Volume<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Volume, m_to_local, m_bbox)
    MI_IMPORT_TYPES(Volume, VolumeGrid)

    SphericalCoordsVolume(const Properties &props) : Base(props) {
        m_volume = props.volume<Volume>("volume", 1.f);

        m_rmin = props.get<ScalarFloat>("rmin", 0.f);
        m_rmax = props.get<ScalarFloat>("rmax", 1.f);
        if (m_rmin > m_rmax)
            Throw("rmin must be lower than rmax!");

        m_fillmin = props.get<ScalarFloat>("fillmin", 0.f);
        m_fillmax = props.get<ScalarFloat>("fillmax", 0.f);

        m_to_local = props.get<ScalarTransform4f>("to_world", ScalarTransform4f()).inverse();
        update_bbox_sphere();
    }

    UnpolarizedSpectrum eval(const Interaction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Point3f p = m_to_local * it.p;
        Float r = dr::norm(p);

        Point3f p_spherical = Point3f(
            (r - m_rmin) / (m_rmax - m_rmin),
            dr::acos(p.z() / r) * dr::InvPi<ScalarFloat>,
            dr::atan2(p.y(), p.x()) * dr::InvTwoPi<ScalarFloat> + .5f
        );
        Interaction3f it_spherical = it;
        it_spherical.p = p_spherical;

        return dr::select(r < m_rmin,
            m_fillmin,
            dr::select(r > m_rmax,
                m_fillmax,
                m_volume->eval(it_spherical, active)
            )
        );
    }

    Float eval_1(const Interaction3f &it, Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Point3f p = m_to_local * it.p;
        Float r = dr::norm(p);

        Point3f p_spherical = Point3f(
            (r - m_rmin) / (m_rmax - m_rmin),
            dr::acos(p.z() / r) * dr::InvPi<ScalarFloat>,
            dr::atan2(p.y(), p.x()) * dr::InvTwoPi<ScalarFloat> + .5f
        );
        Interaction3f it_spherical = it;
        it_spherical.p = p_spherical;

        return dr::select(
            r < m_rmin,
            m_fillmin,
            dr::select(
                r > m_rmax,
                m_fillmax,
                m_volume->eval_1(it_spherical, active)
            )
        );
    }

    ScalarFloat max() const override { return m_volume->max(); }

    ScalarVector3i resolution() const override { return m_volume->resolution(); };

    void traverse(TraversalCallback *callback) override {
        callback->put_object("volume", m_volume.get(),
                             +ParamFlags::NonDifferentiable);
        Base::traverse(callback);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SphericalCoordsVolume[" << std::endl
            << "  to_local = " << string::indent(m_to_local, 13) << "," << std::endl
            << "  bbox = " << string::indent(m_bbox) << "," << std::endl
            << "  volume = " << string::indent(m_volume) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    ScalarFloat m_rmin, m_rmax, m_fillmin, m_fillmax;
    ref<Volume> m_volume;

    void update_bbox_sphere() {
        ScalarTransform4f to_world = m_to_local.inverse();
        ScalarPoint3f a = to_world * ScalarPoint3f(-1.f, -1.f, -1.f);
        ScalarPoint3f b = to_world * ScalarPoint3f(1.f, 1.f, 1.f);
        m_bbox = ScalarBoundingBox3f(a, b);
    }
};

MI_IMPLEMENT_CLASS_VARIANT(SphericalCoordsVolume, Volume)
MI_EXPORT_PLUGIN(SphericalCoordsVolume, "SphericalCoordsVolume texture")

NAMESPACE_END(mitsuba)
