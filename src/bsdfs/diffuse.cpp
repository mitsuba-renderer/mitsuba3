#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/*!

.. _bsdf-diffuse:

Smooth diffuse material (`diffuse`)
-------------------------------------

.. list-table::
 :widths: 20 15 65
 :header-rows: 1
 :class: paramstable

 * - Parameter
   - Type
   - Description
 * - reflectance
   - Spectrum or Texture
   - Specifies the diffuse albedo of the material (Default: 0.5)

The smooth diffuse material (also referred to as *Lambertian*)
represents an ideally diffuse material with a user-specified amount of
reflectance. Any received illumination is scattered so that the surface
looks the same independently of the direction of observation.


.. subfigstart::

.. _fig-diffuse-plain:

.. figure:: ../resources/data/docs/images/bsdfs/diffuse/plain.jpg
    :alt: Homogeneous reflectance
    :width: 40%
    :align: center

    Homogeneous reflectance

.. _fig-diffuse-textured:

.. figure:: ../resources/data/docs/images/bsdfs/diffuse/textured.jpg
    :alt: Textured reflectance
    :width: 40%
    :align: center

    Textured reflectance

.. subfigend::
    :width: 1.0
    :alt: Example Model Resolutions
    :label: fig-diffuse-bsdf


Apart from a homogeneous reflectance value, the plugin can also accept
a nested or referenced texture map to be used as the source of reflectance
information, which is then mapped onto the shape based on its UV
parameterization. When no parameters are specified, the model uses the default
of 50% reflectance.

Note that this material is one-sided---that is, observed from the
back side, it will be completely black. If this is undesirable,
consider using the :ref:`twosided <bsdf_twosided>` BRDF adapter plugin.
The following XML snippet describes a diffuse material,
whose reflectance is specified as an sRGB color:

.. code-block:: xml
    :name: diffuse-srgb

    <bsdf type="diffuse">
        <srgb name="reflectance" value="0.4, 0.45, 0.52"/>
    </bsdf>


Alternatively, the reflectance can be textured:

.. code-block:: xml
    :name: diffuse-texture

    <bsdf type="diffuse">
        <texture type="bitmap" name="reflectance">
            <string name="filename" value="wood.jpg"/>
        </texture>
    </bsdf>

*/
template <typename Float, typename Spectrum>
class SmoothDiffuse final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(SmoothDiffuse, BSDF)
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    SmoothDiffuse(const Properties &props) : Base(props) {
        m_reflectance = props.texture<Texture>("reflectance", .5f);
        m_flags = BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /* sample1 */, const Point2f &sample2,
                                             Mask active) const override {
        ScopedPhase sp(ProfilerPhase::BSDFSample);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        BSDFSample3f bs = zero<BSDFSample3f>();

        active &= cos_theta_i > 0.f;
        if (unlikely(none_or<false>(active) ||
                     !ctx.is_enabled(BSDFFlags::DiffuseReflection)))
            return { bs, 0.f };

        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::DiffuseReflection;
        bs.sampled_component = 0;

        Spectrum value = m_reflectance->eval(si, active);

        return { bs, select(active && bs.pdf > 0.f, value, 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        ScopedPhase sp(ProfilerPhase::BSDFEvaluate);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Spectrum value = m_reflectance->eval(si, active) *
                         math::InvPi<Float> * cos_theta_o;

        return select(cos_theta_i > 0.f && cos_theta_o > 0.f, value, 0.f);
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask /* active */) const override {
        ScopedPhase sp(ProfilerPhase::BSDFEvaluate);

        if (!ctx.is_enabled(BSDFFlags::DiffuseReflection))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("reflectance", m_reflectance.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothDiffuse[" << std::endl
            << "  reflectance = " << string::indent(m_reflectance) << std::endl
            << "]";
        return oss.str();
    }

private:
    ref<Texture> m_reflectance;
};

MTS_EXPORT_PLUGIN(SmoothDiffuse, "Smooth diffuse material");
NAMESPACE_END(mitsuba)
