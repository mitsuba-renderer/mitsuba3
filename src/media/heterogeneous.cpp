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
    MTS_IMPORT_BASE(Medium)
    MTS_IMPORT_TYPES(Scene, Sampler, Texture, Texture3D)

    HeterogeneousMedium(const Properties &props) : Base(props) {
        for (auto &kv : props.objects()) {
            Texture3D *texture3d = dynamic_cast<Texture3D *>(kv.second.get());
            Texture *texture     = dynamic_cast<Texture *>(kv.second.get());
            if (texture3d) {
                if (kv.first == "albedo") {
                    m_albedo = texture3d;
                } else if (kv.first == "sigma_t") {
                    m_sigmat = texture3d;
                } else if (kv.first == "density") {
                    m_density = texture3d;
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
                } else if (kv.first == "density") {
                    m_density = texture3d_ref;
                }
            }
        }

        m_density_scale      = props.float_("density_scale", 1.0f);
        m_use_ratio_tracking = props.bool_("use_ratio_tracking", false);
        m_use_raymarching    = props.bool_("use_raymarching", false);
        m_step_size_scaling  = props.float_("raymarching_step_scaling", 1.0f);

        // TODO: Should also get the maximum of sigmaT
        m_max_density     = m_density_scale * m_density->max();
        m_inv_max_density = 1.0f / m_max_density;
        m_density_aabb    = m_density->bbox();

        ScalarVector3f diag = m_density_aabb.max - m_density_aabb.min;
        ScalarVector3f res  = ScalarVector3f(m_density->resolution());

        // This is just for information purposes only
        Float step_size = m_step_size_scaling * hmin(diag * rcp(res));
        Log(Info, "Volume resolution %s", res);
        Log(Info, "Step size: %f", step_size);
    }

    MTS_INLINE ScalarFloat get_step_size() const {
        ScalarVector3f diag = m_density_aabb.max - m_density_aabb.min;
        ScalarVector3f res  = ScalarVector3f(m_density->resolution());
        return m_step_size_scaling * hmin(diag * rcp(res));
    }

    Float density(const Ray3f &ray, Float t, Mask active) const {
        Interaction3f it;
        it.p           = ray(t);
        it.wavelengths = ray.wavelengths;
        it.time        = ray.time;
        return m_density_scale * depolarize(m_density->eval(it, active)).coeff(0);
    }

    virtual std::tuple<SurfaceInteraction3f, MediumInteraction3f, Spectrum>
    sample_distance(const Scene *scene, const Ray3f &ray, const Point2f &sample, Sampler *sampler,
                    Mask active) const override {

        // Intersect the ray with the scene
        // (not using short ray optimization for heterogeneous media)
        SurfaceInteraction3f si = scene->ray_intersect(ray, active);

        Spectrum weight(1.0f);
        auto [aabb_its, mint, maxt] = m_density_aabb.ray_intersect(ray);
        maxt                        = enoki::min(maxt, ray.maxt);
        active &= aabb_its;

        mint = enoki::max(ray.mint, mint);

        MediumInteraction3f mi;
        mi.p           = Point3f(0.0f, 0.0f, 0.0f);
        mi.sh_frame    = Frame3f(ray.d);
        mi.wi          = Vector3f(0.0f, 0.0f, 1.0f);
        mi.time        = ray.time;
        mi.wavelengths = ray.wavelengths;
        mi.medium      = nullptr;
        mi.t           = math::Infinity<ScalarFloat>;

        Float mean_sigmat = hmean(depolarize(m_sigmat->eval(mi)));

        if (m_use_raymarching) {
            ScalarFloat step_size      = get_step_size();
            ScalarFloat half_step_size = 0.5f * step_size;

            ScalarVector3f diag   = m_density_aabb.max - m_density_aabb.min;
            ScalarFloat diag_norm = norm(diag);

            Float desired_density = -enoki::log(1 - sample.x());

            int max_steps = diag_norm / step_size + 1;

            Float t_a                = mint;
            Float f_a                = density(ray, t_a, active) * mean_sigmat;
            Float maxt               = enoki::min(maxt, si.t);
            Float integrated_density = zero<Float>();
            Mask reached_density     = false;

            // Jitter: Perform first step outside of loop
            Float t_b         = t_a + sample.y() * step_size;
            Float f_b         = density(ray, t_b, active) * mean_sigmat;
            Float new_density = 0.5f * (t_b - t_a) * (f_a + f_b);
            reached_density |= active && (new_density >= desired_density);
            active &= !(active && (reached_density || (t_b > maxt)));
            masked(integrated_density, active) = new_density;
            masked(t_a, active)                = t_b;
            masked(f_a, active)                = f_b;
            mint                               = t_b;

            for (int i = 1; i < max_steps; ++i) {
                masked(t_b, active) = fmadd(i, step_size, mint);
                masked(f_b, active) = density(ray, t_b, active) * mean_sigmat;
                Float new_density   = fmadd(half_step_size, f_a + f_b, integrated_density);

                reached_density |= active && (new_density >= desired_density);
                active &= !reached_density && (t_b <= maxt);

                masked(integrated_density, active) = new_density;
                masked(t_a, active)                = t_b;
                masked(f_a, active)                = f_b;
            }

            // Solve quadratic equation to get exact intersection location
            Float a = 0.5f * (f_b - f_a);
            Float b = f_a;
            Float c = (integrated_density - desired_density) * rcp(t_b - t_a);
            auto [has_solution, solution1, solution2] = math::solve_quadratic(a, b, c);
            Assert(none(active && reached_density && !has_solution));
            Float interp   = select(solution1 >= 0.f && solution1 <= 1.f, solution1, solution2);
            Float sampledt = t_a + (t_b - t_a) * interp;
            Assert(none(active && reached_density && (interp < 0.f || interp > 1.f)));
            Float f_c = (1 - interp) * f_a + interp * f_b;

            // Update integrated_density for rays which do not interact with the medium
            Mask escaped =
                (!reached_density && (t_b > maxt)) || (reached_density && (sampledt > maxt));
            masked(integrated_density, escaped) +=
                0.5f *
                (f_a + density(ray, maxt - math::Epsilon<ScalarFloat>, active) * mean_sigmat);

            // Record medium interaction if the generated "t" is within the range of valid "t"
            Mask valid_mi = reached_density && (sampledt <= maxt);
            Assert(none(valid_mi && f_c <= 0.f));

            // Compute transmittance
            Float tr                             = exp(-integrated_density);
            masked(integrated_density, valid_mi) = desired_density;
            masked(mi.t, valid_mi)               = detach(sampledt);
            masked(mi.p, valid_mi)               = ray(mi.t);
            masked(mi.medium, valid_mi)          = this;

            Float pdf_tr = f_c * tr;
            masked(weight, valid_mi) *=
                m_albedo->eval(mi, valid_mi) * select(pdf_tr > 0.f, f_c * tr / detach(pdf_tr), 1.f);
            masked(si.t, mi.is_valid()) = math::Infinity<ScalarFloat>;
            masked(weight, si.is_valid()) *= select(tr > 0.f, tr / detach(tr), 1.0f);
            return std::make_tuple(si, mi, weight);
        } // ------ done ray marching

        // Delta tracking
        Float t = mint;
        while (any(active)) {

            // 1. Sample tentative free-flight distance using max. density
            Float u = sampler->next_1d(active);
            t += -enoki::log(1 - u) * m_inv_max_density;

            // 2. If sampled distance exceeds distance to surface:
            active &= (t <= si.t) && (t <= maxt);

            // 3. Check whether we sampled a real or virtual particle interaction
            Float d = density(ray, t, active) * mean_sigmat;

            Float p_s     = d * m_inv_max_density;
            Float p_n     = 1.f - p_s;
            Mask valid_mi = active && (sampler->next_1d(active) < p_s);

            masked(mi.t, valid_mi)      = t;
            masked(mi.p, valid_mi)      = ray(mi.t);
            masked(mi.medium, valid_mi) = this;
            masked(weight, valid_mi) *= p_s / detach(p_s) * m_albedo->eval(mi, valid_mi);
            masked(weight, active && !valid_mi) *= p_n / detach(p_n);
            active &= !valid_mi;
        }
        masked(si.t, mi.is_valid()) = math::Infinity<ScalarFloat>;
        return std::make_tuple(si, mi, weight);
    }

    Spectrum eval_transmittance(const Ray3f &ray, Sampler *sampler, Mask active) const override {

        auto [aabb_its, mint, maxt] = m_density_aabb.ray_intersect(ray);

        maxt = enoki::min(maxt, ray.maxt);
        mint = enoki::max(ray.mint, mint);
        active &= aabb_its;

        Float t = mint;

        MediumInteraction3f mi;
        mi.p              = ray.o;
        mi.wavelengths    = ray.wavelengths;
        Float mean_sigmat = hmean(depolarize(m_sigmat->eval(mi)));

        Spectrum tr(1.0f);
        if (m_use_raymarching) {
            ScalarFloat step_size = get_step_size();
            ScalarVector3f diag   = m_density_aabb.max - m_density_aabb.min;
            ScalarFloat diag_norm = norm(diag);
            int max_steps         = diag_norm / step_size + 1;
            Float t_a             = mint;
            Float f_a             = density(ray, t_a, active) * mean_sigmat;
            Float t_b;
            Float integrated_density = zero<Float>();
            for (int i = 1; i < max_steps; ++i) {
                t_b                       = fmadd(i, step_size, mint);
                Mask reached_maxt         = active && (t_b >= maxt);
                masked(t_b, reached_maxt) = maxt;
                Float f_b                 = density(ray, t_b, active) * mean_sigmat;
                integrated_density += 0.5f * (f_a + f_b) * (t_b - t_a);
                active &= !reached_maxt;
                t_a = t_b;
                f_a = f_b;
            }
            return Spectrum(enoki::exp(-integrated_density));
        }

        while (any(active)) {
            Float u = sampler->next_1d(active);
            t += -enoki::log(1 - u) * m_inv_max_density;
            active &= t <= maxt;

            // Float p_s = density(ray, t, active) * mean_sigmat * m_inv_max_density;
            Float p_n = 1.f - density(ray, t, active) * mean_sigmat * m_inv_max_density;
            if (m_use_ratio_tracking) {
                masked(tr, active) *= p_n;
            } else {
                Mask valid_mi = active && ((1.f - p_n) > sampler->next_1d(active));
                masked(tr, valid_mi) *= 0.0f;
                masked(tr, active && !valid_mi) *= p_n / detach(p_n);
                active &= !valid_mi;
            }
        }
        return tr;
    }

    bool is_homogeneous() const override { return false; }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("density", m_density.get());
        callback->put_object("albedo", m_albedo.get());
        callback->put_object("sigma_t", m_sigmat.get());
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
    ref<Texture3D> m_sigmat, m_albedo, m_density;

    ScalarBoundingBox3f m_density_aabb;
    ScalarFloat m_density_scale, m_max_density, m_inv_max_density, m_step_size_scaling;
    bool m_use_ratio_tracking, m_use_raymarching;
};

MTS_IMPLEMENT_CLASS_VARIANT(HeterogeneousMedium, Medium)
MTS_EXPORT_PLUGIN(HeterogeneousMedium, "Heterogeneous Medium")
NAMESPACE_END(mitsuba)
