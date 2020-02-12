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
class HomogeneousMedium final : public Medium<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction)
    MTS_IMPORT_TYPES(Scene, Sampler, Texture, Texture3D)

    HomogeneousMedium(const Properties &props) : Base(props) {
        m_is_homogeneous = true;
        for (auto &kv : props.objects()) {
            Texture3D *texture3d = dynamic_cast<Texture3D *>(kv.second.get());
            Texture *texture     = dynamic_cast<Texture *>(kv.second.get());
            if (texture3d) {
                if (kv.first == "albedo") {
                    m_albedo = texture3d;
                } else if (kv.first == "sigma_t") {
                    m_sigmat = texture3d;
                }
            } else if (texture) {
                // If we directly specified RGB values: automatically convert to
                // a constant Texture
                Properties props2("constant3d");
                ref<Object> texture_ref =
                    props.texture<Texture>(kv.first).get();
                props2.set_object("color", texture_ref);
                ref<Texture3D> texture3d_ref =
                    PluginManager::instance()->create_object<Texture3D>(props2);
                if (kv.first == "albedo") {
                    m_albedo = texture3d_ref;
                } else if (kv.first == "sigma_t") {
                    m_sigmat = texture3d_ref;
                }
            }
        }
        m_density = props.float_("density", 1.0f);
        m_has_spectral_extinction = props.bool_("has_spectral_extinction", true);
    }

    MTS_INLINE auto eval_sigmat(const MediumInteraction3f &mi) const {
        return m_sigmat->eval(mi) * m_density;
    }

    UnpolarizedSpectrum
    get_combined_extinction(const MediumInteraction3f &mi,
                            Mask /* active */) const override {
        return eval_sigmat(mi);
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        auto sigmat                = eval_sigmat(mi);
        auto sigmas                = sigmat * m_albedo->eval(mi, active);
        UnpolarizedSpectrum sigman = 0.f;
        return { sigmas, sigman, sigmat };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f & /* ray */) const override {
        return { true, 0.f, math::Infinity<Float> };
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("density", m_density);
        callback->put_object("albedo", m_albedo.get());
        callback->put_object("sigma_t", m_sigmat.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HomogeneousMedium[" << std::endl
            << "  albedo  = " << string::indent(m_albedo) << std::endl
            << "  sigma_t = " << string::indent(m_sigmat) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture3D> m_sigmat, m_albedo;
    ScalarFloat m_density;
};

MTS_IMPLEMENT_CLASS_VARIANT(HomogeneousMedium, Medium)
MTS_EXPORT_PLUGIN(HomogeneousMedium, "Homogeneous Medium")
NAMESPACE_END(mitsuba)
