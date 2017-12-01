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
        size_t shading_samples = props.long_("shading_samples", 1);

        /// Number of samples to take using the emitter sampling technique
        m_emitter_samples = props.long_("emitter_samples", shading_samples);
        /// Number of samples to take using the BSDF sampling technique
        m_bsdf_samples = props.long_("bsdf_samples", shading_samples);

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
        if (m_emitter_samples > 1)
            sampler->request_2d_array(m_emitter_samples);
        if (m_bsdf_samples > 1)
            sampler->request_2d_array(m_bsdf_samples);
    }
    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Integrator interface
    // =============================================================

    SpectrumfP Li(const RayDifferential3fP &/*r*/, RadianceRecord3fP &/*r_rec*/,
                  const mask_t<FloatP> &/*active*/) const override {
        NotImplementedError("Li (vectorized)");
        return SpectrumfP(0.0f);
    }


    Spectrumf Li(const RayDifferential3f &r, RadianceRecord3f &r_rec) const override {
        // Some aliases and local variables
        const Scene *scene = r_rec.scene;
        auto &its = r_rec.its;
        RayDifferential3f ray(r);
        Spectrumf Li(0.0f);
        Point2f sample;

        // Perform the first ray intersection.
        if (!r_rec.ray_intersect(ray, true)) {
            // If no intersection could be found, possibly return radiance from
            // a background emitter.
            if (m_hide_emitters)
                return Spectrumf(0.0f);
            return scene->eval_environment(ray);
        }

        if (!its.shape) {
            Throw("There was an intersection, but the intersection record"
                  " wasn't properly filled (`its.shape` is empty).");
        }

        // Include emitted radiance
        if (its.is_emitter() && !m_hide_emitters)
            Li += its.Le(-ray.d);

        // // Include radiance from a subsurface scattering model
        // if (its.has_subsurface())
        //     Li += its.Lo_sub(scene, r_rec.sampler, -ray.d, r_rec.depth);

        const BSDF *bsdf = its.bsdf(ray);

        /* Only render the direct illumination component if:
         *
         * 1. The surface has an associated BSDF (i.e. it isn't an index-
         *    matched medium transition -- this is not supported by 'direct')
         * 2. If 'strict_normals'=true, when the geometric and shading
         *    normals classify the incident direction to the same side
         *
         * Otherwise, we can stop right there.
         */
        if (bsdf && m_strict_normals
            && dot(ray.d, its.n) * Frame3f::cos_theta(its.wi) >= 0) {
            return Li;
        }

        /* ================================================================== */
        /*                          Emitter sampling                          */
        /* ================================================================== */
        // TODO: notion of adaptive query (r_rec.extra & RadianceRecord3f::EAdaptiveQuery).
        bool adaptive_query = false;

        // Figure out how many BSDF and direct illumination samples to
        // generate, and where the random numbers should come from.
        Point2f *sample_array;
        size_t n_direct_samples = m_emitter_samples,
               n_bsdf_samples = m_bsdf_samples;
        Float frac_lum = m_frac_lum, frac_bsdf = m_frac_bsdf,
              weight_lum = m_weight_lum, weight_bsdf = m_weight_bsdf;

        if (r_rec.depth > 1 || adaptive_query) {
            // This integrator is used recursively by another integrator.
            // Be less accurate as this sample will not directly be observed.
            n_bsdf_samples = n_direct_samples = 1;
            frac_lum = frac_bsdf = .5f;
            weight_lum = weight_bsdf = 1.0f;
        }

        if (n_direct_samples > 1) {
            sample_array = r_rec.sampler->next_2d_array(n_direct_samples);
        } else {
            sample = r_rec.next_sample_2d();
            sample_array = &sample;
        }

        DirectSample3f d_rec(its);
        if (bsdf->flags() & BSDF::ESmooth) {
            // Only use direct illumination sampling when the surface's
            // BSDF has smooth (i.e. non-Dirac delta) component.
            for (size_t i = 0; i < n_direct_samples; ++i) {
                // Estimate the direct illumination
                Spectrumf value = scene->sample_emitter_direct(d_rec,
                                                               sample_array[i]);

                if (any(value != 0)) {
                    auto emitter = static_cast<const Emitter *>(d_rec.object);

                    // Allocate a record for querying the BSDF
                    BSDFSample3f b_rec(its, its.to_local(d_rec.d));

                    // Evaluate BSDF * cos(theta)
                    const Spectrumf bsdf_val = bsdf->eval(b_rec);

                    if (any(bsdf_val != 0) && (!m_strict_normals
                        || dot(its.n, d_rec.d)
                           * Frame3f::cos_theta(b_rec.wo) > 0)) {
                        // Calculate the probability of sampling that direction
                        // using BSDF sampling.
                        Float bsdf_pdf = emitter->is_on_surface()
                                       ? bsdf->pdf(b_rec) : 0;

                        // Weight using the power heuristic
                        const Float weight = mi_weight(
                                d_rec.pdf * frac_lum,
                                bsdf_pdf * frac_bsdf) * weight_lum;

                        Li += value * bsdf_val * weight;
                    }
                }
            }
        }

        /* ================================================================== */
        /*                            BSDF sampling                           */
        /* ================================================================== */

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
                && wo_dot_geo_n * Frame3f::cos_theta(b_rec.wo) <= 0.0f)
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

            /* Compute the prob. of generating that direction using the
               implemented direct illumination sampling technique */
            const Float lum_pdf = (!(b_rec.sampled_type & BSDF::EDelta)) ?
                    scene->pdf_emitter_direct(d_rec) : 0;

            // Weight using the power heuristic
            const Float weight = mi_weight(
                    bsdf_pdf * frac_bsdf,
                    lum_pdf * frac_lum) * weight_bsdf;

            Li += value * bsdf_val * weight;
        }

        return Li;
    }

    inline Float mi_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a; pdf_b *= pdf_b;
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
