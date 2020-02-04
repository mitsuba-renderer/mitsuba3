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
    MTS_IMPORT_BASE(Medium)
    MTS_IMPORT_TYPES(Scene, Sampler, Texture, Texture3D)

    HomogeneousMedium(const Properties &props) : Base(props) {

        for (auto &kv : props.objects()) {
            Texture3D *texture3d = dynamic_cast<Texture3D *>(kv.second.get());
            Texture *texture     = dynamic_cast<Texture *>(kv.second.get());
            if (texture3d) {
                if (kv.first == "albedo") {
                    m_albedo = texture3d;
                } else if (kv.first == "sigma_t") {
                    m_sigmat = texture3d;
                }
            } else if (texture) { // If we directly specified RGB values: automatically convert to a
                                  // constant Texture3D
                Properties props2("constant3d");
                ref<Object> texture_ref = props.texture<Texture>(kv.first).get();
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
    }

    MTS_INLINE Spectrum eval_sigmat(const MediumInteraction3f &mi) const {
        return m_sigmat->eval(mi) * m_density;
    }

    virtual std::tuple<SurfaceInteraction3f, MediumInteraction3f, Spectrum>
    sample_distance(const Scene *scene, const Ray3f &ray, const Point2f &sample,
                    Sampler * /* sampler */, Mask active) const override {

        MediumInteraction3f mi;
        mi.p            = ray.o;
        mi.wavelengths  = ray.wavelengths;
        Spectrum sigmat = eval_sigmat(mi);

        // Simple importance sampling according to average extinction
        Float avg_sigmat      = hmean(depolarize(sigmat));
        Float u               = sample.x();
        mi.t                  = -enoki::log(1 - u) / avg_sigmat;
        mi.p                  = ray(mi.t);
        mi.sh_frame           = Frame3f(ray.d);
        mi.wi                 = -ray.d;
        mi.time               = ray.time;
        mi.wavelengths        = ray.wavelengths;
        mi.medium             = this;
        masked(mi.t, !active) = math::Infinity<Float>;

        // Intersect the generated short ray(s) with the scene
        Ray3f ray2(ray, ray.mint, mi.t);
        SurfaceInteraction3f si = scene->ray_intersect(ray2, active);
        Spectrum weight(1.0f);
        masked(weight, si.is_valid() && active)  = exp(-si.t * sigmat) / exp(-si.t * avg_sigmat);
        masked(weight, !si.is_valid() && active) = m_albedo->eval(mi) * sigmat * exp(-mi.t * sigmat) / (detach(avg_sigmat) * exp(-mi.t * avg_sigmat));
        masked(si.t, !active)                    = math::Infinity<Float>;

        // Medium interactions are invalid if the surface was intersected
        masked(mi.t, si.is_valid() && active) = math::Infinity<Float>;
        return std::make_tuple(si, mi, weight);
    }

    Spectrum eval_transmittance(const Ray3f &ray, Sampler * /* unused */,
                                Mask /* unused */) const override {

        MediumInteraction3f mi;
        mi.p           = ray.o;
        mi.wavelengths = ray.wavelengths;
        return enoki::exp(-(ray.maxt - ray.mint) * eval_sigmat(mi));
    }

        // NEW INTERFACE
    Spectrum get_combined_extinction(const MediumInteraction3f &mi, Mask /* active */) const override {
        return eval_sigmat(mi);
    }

    std::tuple<Spectrum, Spectrum, Spectrum> get_scattering_coefficients(const MediumInteraction3f &mi, Mask active) const override {
        Spectrum sigmat = eval_sigmat(mi);
        Spectrum sigmas = sigmat * m_albedo->eval(mi, active);
        Spectrum sigman = 0.f;
        return { sigmas, sigman, sigmat };
    }

    std::tuple<Mask, Float, Float> intersect_aabb(const Ray3f & /* ray */) const override {
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
