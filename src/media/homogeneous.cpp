#include <mitsuba/core/frame.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/extremum.h>

NAMESPACE_BEGIN(mitsuba)


/**!

.. _medium-homogeneous:

Homogeneous medium (:monosp:`homogeneous`)
-----------------------------------------------

.. pluginparameters::

 * - albedo
   - |float|, |spectrum| or |volume|
   - Single-scattering albedo of the medium (Default: 0.75).
   - |exposed|, |differentiable|

 * - sigma_t
   - |float| or |spectrum|
   - Extinction coefficient in inverse scene units (Default: 1).
   - |exposed|, |differentiable|

 * - scale
   - |float|
   - Optional scale factor that will be applied to the extinction parameter.
     It is provided for convenience when accommodating data based on different
     units, or to simply tweak the density of the medium. (Default: 1)
   - |exposed|

 * - sample_emitters
   - |bool|
   - Flag to specify whether shadow rays should be cast from inside the volume (Default: |true|)
     If the medium is enclosed in a :ref:`dielectric <bsdf-dielectric>` boundary,
     shadow rays are ineffective and turning them off will significantly reduce
     render time. This can reduce render time up to 50% when rendering objects
     with subsurface scattering.

 * - (Nested plugin)
   - |phase|
   - A nested phase function that describes the directional scattering properties of
     the medium. When none is specified, the renderer will automatically use an instance of
     isotropic.
   - |exposed|, |differentiable|

This class implements a homogeneous participating medium with support for arbitrary
phase functions. This medium can be used to model effects such as fog or subsurface scattering.

The medium is parametrized by the single scattering albedo and the extinction coefficient
:math:`\sigma_t`. The extinction coefficient should be provided in inverse scene units.
For instance, when a world-space distance of 1 unit corresponds to a meter, the
extinction coefficient should have units of inverse meters. For convenience,
the scale parameter can be used to correct the units. For instance, when the scene is in
meters and the coefficients are in inverse millimeters, set scale to 1000.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/medium_homogeneous_sss.jpg
   :caption: Homogeneous medium with constant albedo
.. subfigure:: ../../resources/data/docs/images/render/medium_homogeneous_sss_textured.jpg
   :caption: Homogeneous medium with spatially varying albedo
.. subfigend::
   :label: fig-homogeneous


The homogeneous medium assumes the extinction coefficient to be constant throughout the medium.
However, it supports the use of a spatially varying albedo.

.. tabs::
    .. code-tab:: xml
        :name: lst-homogeneous

        <medium id="myMedium" type="homogeneous">
            <rgb name="albedo" value="0.99, 0.9, 0.96"/>
            <float name="sigma_t" value="5"/>

            <!-- The extinction is also allowed to be spectrally varying
                 Since RGB values have to be in the [0, 1]
                <rgb name="sigma_t" value="0.5, 0.25, 0.8"/>
            -->

            <!-- A homogeneous medium needs to have a constant extinction,
                but can have a spatially varying albedo:

                <volume name="albedo" type="gridvolume">
                    <string name="filename" value="albedo.vol"/>
                </volume>
            -->

            <phase type="hg">
                <float name="g" value="0.7"/>
            </phase>
        </medium>

    .. code-tab:: python

        'type': 'homogeneous',
        'albedo': {
            'type': 'rgb',
            'value': [0.99, 0.9, 0.96]
        },
        'sigma_t': 5,
        # The extinction is also allowed to be spectrally varying
        # since RGB values have to be in the [0, 1]
        # 'sigma_t': {
        #     'value': [0.5, 0.25, 0.8]
        # }

        # A homogeneous medium needs to have a constant extinction,
        # but can have a spatially varying albedo:
        # 'albedo': {
        #     'type': 'gridvolume',
        #     'filename': 'albedo.vol'
        # }

        'phase': {
            'type': 'hg',
            'g': 0.7
        }
*/

template <typename Float, typename Spectrum>
class HomogeneousMedium final : public Medium<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction,
                   m_phase_function, m_extremum)
    MI_IMPORT_TYPES(Scene, Sampler, Texture, Volume, Extremum)

    HomogeneousMedium(const Properties &props) : Base(props) {
        m_is_homogeneous = true;
        m_albedo = props.get_volume<Volume>("albedo", 0.75f);
        m_sigmat = props.get_volume<Volume>("sigma_t", 1.0f);

        m_scale = props.get<ScalarFloat>("scale", 1.0f);
        m_has_spectral_extinction = props.get<bool>("has_spectral_extinction", true);

        // Create a default global extremum structure
        m_extremum =
            PluginManager::instance()->create_object<Extremum>(Properties("globalextremum"));

        ScalarBoundingBox3f extr_bbox(-dr::Infinity<ScalarFloat>, dr::Infinity<ScalarFloat>);
        m_extremum->update_extremum(extr_bbox, m_sigmat.get(), m_scale);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("scale",   m_scale,  ParamFlags::NonDifferentiable);
        cb->put("albedo",  m_albedo, ParamFlags::Differentiable);
        cb->put("sigma_t", m_sigmat, ParamFlags::Differentiable);
        Base::traverse(cb);
    }

    void parameters_changed(const std::vector<std::string> &keys = {}) override {
        if (keys.empty() || string::contains(keys, "sigma_t")) {
            ScalarBoundingBox3f extr_bbox(-dr::Infinity<ScalarFloat>, dr::Infinity<ScalarFloat>);
            m_extremum->update_extremum(extr_bbox, m_sigmat.get(), std::nullopt);
        }

        if (keys.empty() || string::contains(keys, "scale"))
            m_extremum->set_scale(m_scale);
    }

    MI_INLINE auto eval_sigmat(const MediumInteraction3f &mi, Mask active) const {
        auto sigmat = m_sigmat->eval(mi) * m_scale;
        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigmat *= m_phase_function->projected_area(mi, active);
        return sigmat;
    }

    UnpolarizedSpectrum
    get_majorant(const MediumInteraction3f &mi,
                 Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        return eval_sigmat(mi, active) & active;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        auto sigmat                = eval_sigmat(mi, active);
        auto sigmas                = sigmat * m_albedo->eval(mi, active);
        UnpolarizedSpectrum sigman = 0.f;

        return { sigmas & active, sigman, sigmat & active };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f & /* ray */) const override {
        return { true, 0.f, dr::Infinity<Float> };
    }

    std::pair<MediumSample3f, UnpolarizedSpectrum>
    sample_real_interaction(const Ray3f &ray, Float sample,
                            const UInt32 &channel,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, active);

        auto [mei, sigma_t, maxt] = prepare(ray, active);

        Float t = -dr::log(1.f - sample) / index_spectrum(sigma_t, channel);
        Mask real = active && (t <= maxt);
        Float dt = dr::select(real, t, maxt);

        dr::masked(mei.t, real) = t;
        dr::masked(mei.p, real) = ray(t);

        UnpolarizedSpectrum tr = transmittance(sigma_t, dt);
        UnpolarizedSpectrum p = tr, f = tr;
        if (dr::any_or<true>(real)) {
            auto [sigma_s, sigma_n, sigma_t_] =
                get_scattering_coefficients(mei, real);
            DRJIT_MARK_USED(sigma_n);
            DRJIT_MARK_USED(sigma_t_);
            dr::masked(p, real) = tr * sigma_t;
            dr::masked(f, real) = tr * sigma_s;
        }

        Float p_c   = index_spectrum(p, channel);
        Float inv_p = dr::select(p_c > 0.f, dr::rcp(p_c), 0.f);

        MediumSample3f ms;
        ms.mei           = mei;
        ms.pdf_ratio     = dr::select(active, p * inv_p, 1.f);
        ms.transmittance = dr::select(active, f * inv_p, 1.f);
        return { ms, ms.transmittance };
    }

    UnpolarizedSpectrum
    sample_transmittance(const Ray3f &ray, Float /*sample*/,
                         const UInt32 & /*channel*/,
                         Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        auto [mei, sigma_t, maxt] = prepare(ray, active);
        DRJIT_MARK_USED(mei);
        return dr::select(active, transmittance(sigma_t, maxt), 1.f);
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    sample_transmittance_pdf(const Ray3f &ray, Float sample,
                             const UInt32 &channel,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        auto [mei, sigma_t, maxt] = prepare(ray, active);
        DRJIT_MARK_USED(mei);

        Float t = -dr::log(1.f - sample) / index_spectrum(sigma_t, channel);
        Mask crossed = !(t <= maxt);

        UnpolarizedSpectrum tr = transmittance(sigma_t, maxt);
        Float p_c   = index_spectrum(tr, channel);
        Float inv_p = dr::select(p_c > 0.f, dr::rcp(p_c), 0.f);

        UnpolarizedSpectrum weight =
            dr::select(active, dr::select(crossed, tr * inv_p, 0.f), 1.f);
        return { weight, weight, dr::select(active, tr * inv_p, 1.f) };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HomogeneousMedium[" << std::endl
            << "  albedo = " << string::indent(m_albedo) << "," << std::endl
            << "  sigma_t = " << string::indent(m_sigmat) << "," << std::endl
            << "  scale = " << string::indent(m_scale)  << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(HomogeneousMedium)
private:
    MI_INLINE Float index_spectrum(const UnpolarizedSpectrum &spec,
                                   const UInt32 &idx) const {
        Float m = spec[0];
        if constexpr (is_rgb_v<Spectrum>) {
            dr::masked(m, idx == 1u) = spec[1];
            dr::masked(m, idx == 2u) = spec[2];
        } else {
            DRJIT_MARK_USED(idx);
        }
        return m;
    }

    MI_INLINE std::tuple<MediumInteraction3f, UnpolarizedSpectrum, Float>
    prepare(const Ray3f &ray, Mask active) const {
        MediumInteraction3f mei = dr::zeros<MediumInteraction3f>();
        mei.t           = dr::Infinity<Float>;
        mei.p           = ray.o;
        mei.wi          = -ray.d;
        mei.sh_frame    = Frame3f(mei.wi);
        mei.time        = ray.time;
        mei.wavelengths = ray.wavelengths;
        mei.medium      = this;

        UnpolarizedSpectrum sigma_t = eval_sigmat(mei, active);
        return { mei, sigma_t, dr::maximum(ray.maxt, 0.f) };
    }

    MI_INLINE UnpolarizedSpectrum
    transmittance(const UnpolarizedSpectrum &sigma_t, const Float &dt) const {
        return dr::exp(-dr::select(sigma_t > 0.f, sigma_t * dt, 0.f));
    }

    ref<Volume> m_sigmat, m_albedo;
    ScalarFloat m_scale;

    MI_TRAVERSE_CB(Base, m_sigmat, m_albedo, m_scale)
};

MI_EXPORT_PLUGIN(HomogeneousMedium)
NAMESPACE_END(mitsuba)
