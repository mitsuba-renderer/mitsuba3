#include <enoki/stl.h>

#include <mitsuba/core/frame.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class HeterogeneousMedium final : public Medium<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction)
    MTS_IMPORT_TYPES(Scene, Sampler, Texture, Volume)

    HeterogeneousMedium(const Properties &props) : Base(props) {
        m_is_homogeneous = false;
        m_albedo = props.volume<Volume>("albedo", 0.75f);
        m_sigmat = props.volume<Volume>("sigma_t", 1.f);

        m_scale = props.float_("scale", 1.0f);
        m_has_spectral_extinction = props.bool_("has_spectral_extinction", true);

        m_max_density = m_scale * m_sigmat->max();
        m_aabb        = m_sigmat->bbox();
    }

    UnpolarizedSpectrum
    get_combined_extinction(const MediumInteraction3f & /* mi */,
                            Mask active) const override {
        // TODO: This could be a spectral quantity (at least in RGB mode)
        MTS_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        return m_max_density;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        auto sigmat = m_scale * m_sigmat->eval(mi, active);
        auto sigmas = sigmat * m_albedo->eval(mi, active);
        auto sigman = get_combined_extinction(mi, active) - sigmat;
        return { sigmas, sigman, sigmat };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_aabb.ray_intersect(ray);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("scale", m_scale);
        callback->put_object("albedo", m_albedo.get());
        callback->put_object("sigma_t", m_sigmat.get());
        Base::traverse(callback);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HeterogeneousMedium[" << std::endl
            << "  albedo  = " << string::indent(m_albedo) << std::endl
            << "  sigma_t = " << string::indent(m_sigmat) << std::endl
            << "  scale   = " << string::indent(m_scale) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Volume> m_sigmat, m_albedo;
    ScalarFloat m_scale;

    ScalarBoundingBox3f m_aabb;
    ScalarFloat m_max_density;
};

MTS_IMPLEMENT_CLASS_VARIANT(HeterogeneousMedium, Medium)
MTS_EXPORT_PLUGIN(HeterogeneousMedium, "Heterogeneous Medium")
NAMESPACE_END(mitsuba)
