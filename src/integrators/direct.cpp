#include <mitsuba/core/properties.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/*! \plugin{direct}{Direct illumination integrator}
 * \order{1}
 * \parameters{
 *     \parameter{shading_samples}{\Integer}{This convenience parameter can be
 *         used to set both \code{emitter_samples} and \code{bsdf_samples} at
 *         the same time.
 *     }
 *     \parameter{emitter_samples}{\Integer}{Optional more fine-grained
 *        parameter: specifies the number of samples that should be generated
 *        using the direct illumination strategies implemented by the scene's
 *        emitters\default{set to the value of \code{shading_samples}}
 *     }
 *     \parameter{bsdf_samples}{\Integer}{Optional more fine-grained
 *        parameter: specifies the number of samples that should be generated
 *        using the BSDF sampling strategies implemented by the scene's
 *        surfaces\default{set to the value of \code{shading_samples}}
 *     }
 *     \parameter{strict_normals}{\Boolean}{Be strict about potential
 *        inconsistencies involving shading normals? See
 *        page~\pageref{sec:strictnormals} for details.
 *        \default{no, i.e. \code{false}}
 *     }
 *     \parameter{hide_emitters}{\Boolean}{Hide directly visible emitters?
 *        See page~\pageref{sec:hideemitters} for details.
 *        \default{no, i.e. \code{false}}
 *     }
 * }
 * \vspace{-1mm}
 * \renderings{
 *     \medrendering{Only BSDF sampling}{integrator_direct_bsdf}
 *     \medrendering{Only emitter sampling}{integrator_direct_lum}
 *     \medrendering{BSDF and emitter sampling}{integrator_direct_both}
 *     \caption{
 *         \label{fig:integrator-direct}
 *         This plugin implements two different strategies for computing the
 *         direct illumination on surfaces. Both of them are dynamically
 *         combined then obtain a robust rendering algorithm.
 *     }
 * }
 *
 * This integrator implements a direct illumination technique that makes use
 * of \emph{multiple importance sampling}: for each pixel sample, the
 * integrator generates a user-specifiable number of BSDF and emitter
 * samples and combines them using the power heuristic. Usually, the BSDF
 * sampling technique works very well on glossy objects but does badly
 * everywhere else (\subfigref{integrator-direct}{a}), while the opposite
 * is true for the emitter sampling technique
 * (\subfigref{integrator-direct}{b}). By combining these approaches, one
 * can obtain a rendering technique that works well in both cases
 * (\subfigref{integrator-direct}{c}).
 *
 * The number of samples spent on either technique is configurable, hence
 * it is also possible to turn this plugin into an emitter sampling-only
 * or BSDF sampling-only integrator.
 *
 * For best results, combine the direct illumination integrator with the
 * low-discrepancy sample generator (\code{ldsampler}). Generally, the number
 * of pixel samples of the sample generator can be kept relatively
 * low (e.g. \code{sampleCount=4}), whereas the \code{shading_samples}
 * parameter of this integrator should be increased until the variance in
 * the output renderings is acceptable.
 *
 * \remarks{
 *    \item This integrator does not handle participating media or
 *          indirect illumination.
 * }
 */
class MIDirectIntegrator : public SamplingIntegrator {
public:
    // =============================================================
    //! @{ \name Constructors
    // =============================================================
    MIDirectIntegrator(const Properties &props) : SamplingIntegrator(props) {
        if (props.has_property("shading_samples")
            && (props.has_property("emitter_samples")
                || props.has_property("bsdf_samples"))) {
            Log(EError, "Cannot specify both 'shading_samples' and"
                        " ('emitter_samples' and/or 'bsdf_samples').");
        }

        /// Number of shading samples -- this parameter is a shorthand notation
        /// to set both 'emitter_samples' and 'bsdf_samples' at the same time
        size_t shading_samples = props.size_("shading_samples", 1);

        /// Number of samples to take using the emitter sampling technique
        m_emitter_samples = props.size_("emitter_samples", shading_samples);
        /// Number of samples to take using the BSDF sampling technique
        m_bsdf_samples = props.size_("bsdf_samples", shading_samples);

        /// Be strict about potential inconsistencies involving shading normals?
        m_strict_normals = props.bool_("strict_normals", false);
        /// When this flag is set to true, contributions from directly
        /// visible emitters will not be included in the rendered image
        m_hide_emitters = props.bool_("hide_emitters", false);

        Assert(m_emitter_samples + m_bsdf_samples > 0);
        configure();
    }

    // /// Unserialize from a binary data stream
    // MIDirectIntegrator(Stream *stream, InstanceManager *manager)
    //  : SamplingIntegrator(stream, manager) {
    //     m_emitter_samples = stream->read_size();
    //     m_bsdf_samples = stream->read_size();
    //     m_strict_normals = stream->read_bool();
    //     m_hide_emitters = stream->read_bool();
    //     configure();
    // }

    // void serialize(Stream *stream, InstanceManager *manager) const {
    //     SamplingIntegrator::serialize(stream, manager);
    //     stream->write_size(m_emitter_samples);
    //     stream->write_size(m_bsdf_samples);
    //     stream->write_bool(m_strict_normals);
    //     stream->write_bool(m_hide_emitters);
    // }
    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Configuration
    // =============================================================
    void configure() {
        size_t sum = m_emitter_samples + m_bsdf_samples;
        m_weight_bsdf = 1 / (Float) m_bsdf_samples;
        m_weight_lum = 1 / (Float) m_emitter_samples;
        m_frac_bsdf = m_bsdf_samples / (Float) sum;
        m_frac_lum = m_emitter_samples / (Float) sum;
    }

    void configure_sampler(const Scene *scene, Sampler *sampler) override {
        SamplingIntegrator::configure_sampler(scene, sampler);
        if (m_emitter_samples > 1) {
            // TODO: avoid having to request both kinds of arrays, even though
            // we don't know yet if we're going to use the scalar or vector
            // variant of the integrator.
            sampler->request_2d_array(m_emitter_samples);
            sampler->request_2d_array_p(m_emitter_samples);
        }
        if (m_bsdf_samples > 1) {
            // TODO: same as above.
            sampler->request_2d_array(m_bsdf_samples);
            sampler->request_2d_array_p(m_bsdf_samples);
        }
    }
    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Integrator interface
    // =============================================================

    template <typename RayDifferential,
              typename Point3 = typename RayDifferential::Point>
    auto Li_impl(const RayDifferential &r, RadianceSample<Point3> &r_rec,
                 const mask_t<value_t<Point3>> &active_) const {
        using Value        = value_t<Point3>;
        using Spectrum     = Spectrum<Value>;
        using Mask         = mask_t<Value>;
        using Point2       = point2_t<Point3>;
        using Frame        = Frame<Point3>;
        using BSDFPtr      = like_t<Value, const BSDF *>;
        using EmitterPtr   = like_t<Value, const Emitter *>;
        using BSDFSample   = BSDFSample<Point3>;
        using DirectSample = DirectSample<Point3>;

        // Some aliases and local variables
        const Scene *scene = r_rec.scene;
        auto &its = r_rec.its;
        RayDifferential ray(r);
        Spectrum Li(0.0f);
        Point2 sample;
        Mask active(active_);

        // Perform the first ray intersection.
        auto hit = r_rec.ray_intersect(ray, active);
        // Even if there's no intersection, possibly add radiance from
        // a background emitter.
        if (!m_hide_emitters)
            masked(Li, ~hit) = scene->eval_environment(ray);
        active &= hit;
        if (none(active))  // Early return
            return Li;

        Assert(none(active & eq(its.shape, nullptr)),
               "There was an intersection, but the intersection record"
               " wasn't properly filled (`its.shape` is null).");

        // Include emitted radiance
        auto is_emitter = active & (!m_hide_emitters) & its.is_emitter(active);
        if (any(is_emitter))
            masked(Li, is_emitter) += its.Le(-ray.d, is_emitter);

        // // Include radiance from a subsurface scattering model
        // if (its.has_subsurface())
        //     Li += its.Lo_sub(scene, r_rec.sampler, -ray.d, r_rec.depth);

        BSDFPtr bsdf = its.bsdf(ray, active);

        /* Only render the direct illumination component if:
         *
         * 1. The surface has an associated BSDF (i.e. it isn't an index-
         *    matched medium transition -- this is not supported by 'direct')
         * 2. If 'strict_normals'=true, when the geometric and shading
         *    normals classify the incident direction to the same side
         *
         * Otherwise, we can stop right there.
         */
        Assert(none(active & eq(bsdf, nullptr)), "Shapes without BSDF are not"
               " supported by the Direct integrator.");
        if (m_strict_normals)
            active &= (dot(ray.d, its.n) * Frame::cos_theta(its.wi) < 0);
        if (none(active))  // Early return
            return Li;


        /* ================================================================== */
        /*                          Emitter sampling                          */
        /* ================================================================== */

        // Figure out how many BSDF and direct illumination samples to
        // generate, and where the random numbers should come from.
        Point2 *sample_array;
        size_t n_direct_samples = m_emitter_samples,
               n_bsdf_samples = m_bsdf_samples;
        Float frac_lum = m_frac_lum, frac_bsdf = m_frac_bsdf,
              weight_lum = m_weight_lum;  //, weight_bsdf = m_weight_bsdf;

        // Get samples for direct illumination
        if (n_direct_samples > 1) {
            sample_array = r_rec.sampler->template next_array<Point2>(n_direct_samples);
        } else {
            sample = r_rec.next_sample_2d();
            sample_array = &sample;
        }

        DirectSample d_rec(its);
        // Only use direct illumination sampling when the surface's
        // BSDF has smooth (i.e. non-Dirac delta) component.
        // TODO: OR-ing flags, getting a mask out.
        auto sample_direct = active & neq(bsdf->flags(active) & BSDF::ESmooth,
                                          (uint32_t) 0);
        if (any(sample_direct)) {
            for (size_t i = 0; i < n_direct_samples; ++i) {
                // Estimate the direct illumination
                Spectrum value = scene->sample_emitter_direct(
                        d_rec, sample_array[i], true, active);

                Mask enabled = active & any(neq(value, 0.0f));
                if (none(enabled))
                    continue;

                auto emitter = reinterpret_array<EmitterPtr>(d_rec.object);

                // Allocate a record for querying the BSDF
                BSDFSample b_rec(its, its.to_local(d_rec.d));

                // Evaluate BSDF * cos(theta)
                Spectrum bsdf_val = bsdf->eval(b_rec, ESolidAngle, active);
                Mask same_side = (dot(its.n, d_rec.d)
                                  * Frame::cos_theta(b_rec.wo)) >= 0;
                enabled  &= (m_strict_normals ? same_side : true);
                if (none(enabled))
                    continue;  // Early return for this sample

                // Calculate the probability of sampling that direction
                // using BSDF sampling.
                auto bsdf_pdf = select(emitter->is_on_surface(enabled),
                                       bsdf->pdf(b_rec, ESolidAngle, enabled),
                                       0.0f);

                // Weight using the power heuristic
                Value weight = weight_lum * mi_weight(d_rec.pdf * frac_lum,
                                                      bsdf_pdf * frac_bsdf);

                masked(Li, enabled) += value * bsdf_val * weight;
                if (any_nested(enoki::isnan(Li))) {
                    Log(EWarn, "Has nan now");
                }
            }
        }

        /* ================================================================== */
        /*                            BSDF sampling                           */
        /* ================================================================== */
        if (n_bsdf_samples > 0)
            NotImplementedError("BSDF sampling");

        #if 0
        if (n_bsdf_samples > 1) {
            sample_array = r_rec.sampler->next_2d_array(n_bsdf_samples);
        } else {
            sample = r_rec.next_sample_2d();
            sample_array = &sample;
        }

        SurfaceInteraction3f bsdf_its;
        Float mint = 0,
              maxt = std::numeric_limits<Float>::max();
        for (size_t i = 0; i < n_bsdf_samples; ++i) {
            // Sample BSDF * cos(theta) and also request the local density
            BSDFSample3f b_rec(its, r_rec.sampler, ERadiance);
            Float bsdf_pdf;
            Spectrumf bsdf_val;

            std::tie(bsdf_val, bsdf_pdf) = bsdf->sample(b_rec, sample_array[i]);
            if (all(eq(bsdf_val, 0.0f)))
                continue;

            // Prevent light leaks due to the use of shading normals
            const Vector3f wo = its.to_world(b_rec.wo);
            Float wo_dot_geo_n = dot(its.n, wo);
            if (m_strict_normals
                && wo_dot_geo_n * Frame::cos_theta(b_rec.wo) <= 0.0f)
                continue;

            // Trace a ray in this direction
            Ray3f bsdf_ray(its.p, wo);
            bsdf_ray.time = ray.time;
            bool hit;
            Float t;
            std::tie(hit, t) = scene->ray_intersect(bsdf_ray, mint, maxt, bsdf_its, true);

            Spectrumf value;
            if (hit) {
                // Intersected something - check if it was an emitter
                if (!bsdf_its.is_emitter())
                    continue;

                value = bsdf_its.Le(-bsdf_ray.d);
                d_rec.set_query(bsdf_ray, bsdf_its);
            } else {
                // Intersected nothing -- perhaps there is an environment map?
                const Emitter *env = scene->environment_emitter();

                if (!env || (m_hide_emitters && b_rec.sampled_type == BSDF::ENull))
                    continue;

                value = env->eval_environment(RayDifferential3f(bsdf_ray));
                if (!env->fill_direct_sample(d_rec, bsdf_ray))
                    continue;
            }

            // Compute the probability of generating that direction using the
            // implemented direct illumination sampling technique.
            const Float lum_pdf = (!(b_rec.sampled_type & BSDF::EDelta)) ?
                    scene->pdf_emitter_direct(d_rec) : 0;

            // Weight using the power heuristic
            const Float weight =  weight_bsdf * mi_weight(bsdf_pdf * frac_bsdf,
                                                          lum_pdf * frac_lum);

            Li += value * bsdf_val * weight;
        }
        #endif

        return Li;
    }

    MTS_IMPLEMENT_INTEGRATOR()

    template <typename Value>
    Value mi_weight(Value pdf_a, Value pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        return pdf_a / (pdf_a + pdf_b);
    }
    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Misc
    // =============================================================
    std::string to_string() const override {
        using string::indent;

        std::ostringstream oss;
        oss << "MIDirectIntegrator[" << std::endl
            << "  emitter_samples = " << m_emitter_samples << "," << std::endl
            << "  bsdf_samples = " << m_bsdf_samples << "," << std::endl
            << "  strict_normals = " << m_strict_normals << "," << std::endl
            << "  hide_emitters = " << m_hide_emitters << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
    //! @}
    // =============================================================

private:
    size_t m_emitter_samples;
    size_t m_bsdf_samples;
    Float m_frac_bsdf, m_frac_lum;
    Float m_weight_bsdf, m_weight_lum;
    bool m_strict_normals;
    bool m_hide_emitters;
};


MTS_IMPLEMENT_CLASS(MIDirectIntegrator, SamplingIntegrator);
MTS_EXPORT_PLUGIN(MIDirectIntegrator, "Direct integrator");
NAMESPACE_END(mitsuba)
