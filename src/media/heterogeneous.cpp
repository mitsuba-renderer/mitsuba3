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
        for (auto &kv : props.objects()) {
            Volume *volume = dynamic_cast<Volume *>(kv.second.get());
            Texture *texture     = dynamic_cast<Texture *>(kv.second.get());
            if (volume) {
                if (kv.first == "albedo") {
                    m_albedo = volume;
                } else if (kv.first == "sigma_t") {
                    m_sigmat = volume;
                }
            } else if (texture) {
                // If we directly specified RGB values:
                // automatically convert to a constant texture
                Properties props2("constvolume");
                ref<Object> texture_ref = props.texture<Texture>(kv.first).get();
                props2.set_object("color", texture_ref);
                ref<Volume> volume_ref =
                    PluginManager::instance()->create_object<Volume>(props2);
                if (kv.first == "albedo") {
                    m_albedo = volume_ref;
                } else if (kv.first == "sigma_t") {
                    m_sigmat = volume_ref;
                }
            }
        }

        m_density_scale = props.float_("density_scale", 1.0f);
        m_has_spectral_extinction = props.bool_("has_spectral_extinction", true);

        m_max_density = m_density_scale * m_sigmat->max();
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
        auto sigmat = m_density_scale * m_sigmat->eval(mi, active);
        auto sigmas = sigmat * m_albedo->eval(mi, active);
        auto sigman = get_combined_extinction(mi, active) - sigmat;
        return { sigmas, sigman, sigmat };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_aabb.ray_intersect(ray);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("density", m_density.get());
        callback->put_object("albedo", m_albedo.get());
        callback->put_object("sigma_t", m_sigmat.get());
        Base::traverse(callback);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HeterogeneousMedium[" << std::endl
            << "  albedo  = " << string::indent(m_albedo) << std::endl
            << "  sigma_t = " << string::indent(m_sigmat) << std::endl
            << "  density = " << string::indent(m_density) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Volume> m_sigmat, m_albedo, m_density;

    ScalarBoundingBox3f m_aabb;
    ScalarFloat m_density_scale, m_max_density;
};

MTS_IMPLEMENT_CLASS_VARIANT(HeterogeneousMedium, Medium)
MTS_EXPORT_PLUGIN(HeterogeneousMedium, "Heterogeneous Medium")
NAMESPACE_END(mitsuba)
