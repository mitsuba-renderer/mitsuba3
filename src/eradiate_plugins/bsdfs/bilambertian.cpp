#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _plugin-bsdf-bilambertian:

Bi-Lambertian material (:monosp:`bilambertian`)
-----------------------------------------------

.. pluginparameters::

 * - reflectance
   - |spectrum| or |texture|
   - Specifies the diffuse reflectance of the material. Default: 0.5
   - |exposed| |differentiable|

 * - transmittance
   - |spectrum| or |texture|
   - Specifies the diffuse transmittance of the material. Default: 0.5
   - |exposed| |differentiable|

The bi-Lambertian material scatters light diffusely into the entire sphere.
The reflectance specifies the amount of light scattered into the incoming
hemisphere, while the transmittance specifies the amount of light scattered
into the outgoing hemisphere. This material is two-sided.

.. note::

   This material is not designed for realistic rendering, but rather for
   large-scale simulation of atmospheric radiative transfer over vegetated
   surfaces.

*/

template <typename Float, typename Spectrum>
class BiLambertian final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    BiLambertian(const Properties &props) : Base(props) {
        m_reflectance   = props.texture<Texture>("reflectance", .5f);
        m_transmittance = props.texture<Texture>("transmittance", .5f);

        m_components.push_back(BSDFFlags::DiffuseReflection |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::DiffuseTransmission |
                               BSDFFlags::FrontSide | BSDFFlags::BackSide);

        m_flags = m_components[0] | m_components[1];
        dr::set_attr(this, "flags", m_flags);
    }

    std::pair<BSDFSample3f, Spectrum>
    sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
           Float sample1, const Point2f &sample2, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        bool has_reflect  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 0),
             has_transmit = ctx.is_enabled(BSDFFlags::DiffuseTransmission, 1);

        if (unlikely(dr::none_or<false>(active) ||
                     (!has_reflect && !has_transmit)))
            return { dr::zeros<BSDFSample3f>(), UnpolarizedSpectrum(0.f) };

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        Vector3f wo       = warp::square_to_cosine_hemisphere(sample2);

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        UnpolarizedSpectrum value(0.f);

        // Select the lobe to be sampled
        UnpolarizedSpectrum r = m_reflectance->eval(si, active),
                            t = m_transmittance->eval(si, active);

        Float reflection_sampling_weight = dr::mean(r / (r + t)),
              transmission_sampling_weight = 1.f - reflection_sampling_weight;

        // Handle case where r = t = 0
        dr::masked(reflection_sampling_weight,
                   dr::isnan(reflection_sampling_weight))   = 0.f;
        dr::masked(transmission_sampling_weight,
                   dr::isnan(transmission_sampling_weight)) = 0.f;

        Mask selected_r = (sample1 < reflection_sampling_weight) && active,
             selected_t = (sample1 >= reflection_sampling_weight) && active;

        // Evaluate
        value = dr::select(active, Float(1.f), 0.f);
        value[selected_r] *= r / reflection_sampling_weight;
        value[selected_t] *= t / transmission_sampling_weight;

        // Compute PDF
        bs.pdf =
            dr::select(active, warp::square_to_cosine_hemisphere_pdf(wo), 0.f);
        bs.pdf =
            dr::select(selected_r, bs.pdf * reflection_sampling_weight, bs.pdf);
        bs.pdf = dr::select(selected_t, bs.pdf * transmission_sampling_weight,
                            bs.pdf);

        // Set other interaction fields
        bs.eta               = 1.f;
        bs.sampled_component = dr::select(selected_r, UInt32(0), UInt32(1));
        bs.sampled_type =
            dr::select(selected_r, UInt32(+BSDFFlags::DiffuseReflection),
                       UInt32(+BSDFFlags::DiffuseTransmission));

        // Flip the outgoing direction if the incoming comes from "behind"
        wo = dr::select(cos_theta_i > 0, wo, Vector3f(wo.x(), wo.y(), -wo.z()));

        // Flip the outgoing direction if transmission was selected
        bs.wo = dr::select(selected_r, wo, Vector3f(wo.x(), wo.y(), -wo.z()));

        return { bs, dr::select(active && bs.pdf > 0.f,
                                depolarizer<Spectrum>(value), 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_reflect  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 0),
             has_transmit = ctx.is_enabled(BSDFFlags::DiffuseTransmission, 1);

        if (unlikely((!has_reflect && !has_transmit) ||
                     dr::none_or<false>(active)))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        UnpolarizedSpectrum result(0.f);

        if (has_reflect) {
            // If reflection is activated, compute reflection for relevant
            // directions
            auto is_reflect =
                Mask(dr::eq(dr::sign(cos_theta_i), dr::sign(cos_theta_o))) && active;
            result[is_reflect] = m_reflectance->eval(si, is_reflect);
        }

        if (has_transmit) {
            // If transmission is activated, compute transmission for relevant
            // directions
            auto is_transmit =
                Mask(dr::neq(dr::sign(cos_theta_i), dr::sign(cos_theta_o))) && active;
            result[is_transmit] = m_transmittance->eval(si, is_transmit);
        }

        result[active] *= (dr::InvPi<Float> * dr::abs(cos_theta_o));

        return dr::select(active, result, 0.f);
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        bool has_reflect  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 0),
             has_transmit = ctx.is_enabled(BSDFFlags::DiffuseTransmission, 1);

        if (unlikely(dr::none_or<false>(active) ||
                     (!has_reflect && !has_transmit)))
            return 0.f;

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        // Ensure that incoming direction is in upper hemisphere
        Vector3f wo_flip{ wo.x(), wo.y(), dr::abs(cos_theta_o) };

        Float result = dr::select(
            active, warp::square_to_cosine_hemisphere_pdf(wo_flip), 0.f);

        UnpolarizedSpectrum r = m_reflectance->eval(si, active),
                            t = m_transmittance->eval(si, active);

        Float reflection_sampling_weight = dr::mean(r / (r + t)),
              transmission_sampling_weight = 1.f - reflection_sampling_weight;

        // Handle case where r = t = 0
        dr::masked(reflection_sampling_weight,
                   dr::isnan(reflection_sampling_weight))   = 0.f;
        dr::masked(transmission_sampling_weight,
                   dr::isnan(transmission_sampling_weight)) = 0.f;

        if (has_reflect) {
            auto is_reflect =
                Mask(dr::eq(dr::sign(cos_theta_i), dr::sign(cos_theta_o))) && active;
            dr::masked(result, is_reflect) *= reflection_sampling_weight;
        }

        if (has_transmit) {
            auto is_transmit =
                Mask(dr::neq(dr::sign(cos_theta_i), dr::sign(cos_theta_o))) && active;
            dr::masked(result, is_transmit) *= transmission_sampling_weight;
        }

        return result;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("reflectance", m_reflectance.get(), +ParamFlags::Differentiable);
        callback->put_object("transmittance", m_transmittance.get(), +ParamFlags::Differentiable);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Bilambertian[" << std::endl
            << "  reflectance = " << string::indent(m_reflectance) << std::endl
            << "  transmittance = " << string::indent(m_transmittance)
            << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_reflectance;
    ref<Texture> m_transmittance;
};

MI_IMPLEMENT_CLASS_VARIANT(BiLambertian, BSDF)
MI_EXPORT_PLUGIN(BiLambertian, "Bi-Lambertian material")
NAMESPACE_END(mitsuba)
