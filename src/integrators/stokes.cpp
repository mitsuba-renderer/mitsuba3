#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/mueller.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _integrator-stokes:

Stokes vector integrator (:monosp:`stokes`)
-----------------------------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - :paramtype:`integrator`
   - Sub-integrator (only one can be specified) which will be sampled along the Stokes
     integrator. In polarized rendering modes, its output Stokes vector is written
     into distinct images.

This integrator returns a multi-channel image describing the complete measured
polarization state at the sensor, represented as a Stokes vector :math:`\mathbf{s}`.

Here we show an example monochrome output in a scene with two dielectric and one
conductive sphere that all affect the polarization state of the
(initially unpolarized) light.

The first entry corresponds to usual radiance, whereas the remaining three entries
describe the polarization of light shown as false color images (green: positive, red: negative).

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/integrator_stokes_cbox.jpg
   :caption: ":math:`\mathbf{s}_0`": radiance
.. subfigure:: ../../resources/data/docs/images/render/integrator_stokes_cbox_s1.jpg
   :caption: ":math:`\mathbf{s}_1`": horizontal vs. vertical polarization
.. subfigure:: ../../resources/data/docs/images/render/integrator_stokes_cbox_s2.jpg
   :caption: ":math:`\mathbf{s}_2`": positive vs. negative diagonal polarization
.. subfigure:: ../../resources/data/docs/images/render/integrator_stokes_cbox_s3.jpg
   :caption: ":math:`\mathbf{s}_3`": right vs. left circular polarization
.. subfigend::
   :label: fig-stokes

In the following example, a normal path tracer is nested inside the Stokes vector
integrator:

.. tabs::
    .. code-tab::  xml

        <integrator type="stokes">
            <integrator type="path">
                <!-- path tracer parameters -->
            </integrator>
        </integrator>

    .. code-tab:: python

        'type': 'stokes',
        'nested': {
            'type': 'path',
            # .. path tracer parameters
        }

 */

template <typename Float, typename Spectrum>
class StokesIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sampler, Medium)

    StokesIntegrator(const Properties &props) : Base(props) {
        if constexpr (!is_polarized_v<Spectrum>)
            Throw("This integrator should only be used in polarized mode!");
        for (auto &kv : props.objects()) {
            Base *integrator = dynamic_cast<Base *>(kv.second.get());
            if (!integrator)
                Throw("Child objects must be of type 'SamplingIntegrator'!");
            if (m_integrator)
                Throw("More than one sub-integrator specified!");
            m_integrator = integrator;
        }

        if (!m_integrator)
            Throw("Must specify a sub-integrator!");
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * sampler,
                                     const RayDifferential3f &ray,
                                     const Medium *medium,
                                     Float *aovs,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        auto [spec, mask] = m_integrator->sample(scene, sampler, ray, medium, aovs + 12, active);

        if constexpr (is_polarized_v<Spectrum>) {
            /* The Stokes vector that comes from the integrator is still aligned
               with the implicit Stokes frame used for the ray direction. Apply
               one last rotation here s.t. it aligns with the sensor's x-axis. */
            auto sensor = scene->sensors()[0];
            Vector3f current_basis = mueller::stokes_basis(-ray.d);
            Vector3f vertical = sensor->world_transform() * Vector3f(0.f, 1.f, 0.f);
            Vector3f target_basis = dr::cross(ray.d, vertical);
            spec = mueller::rotate_stokes_basis(-ray.d,
                                                 current_basis,
                                                 target_basis) * spec;

            for (int i = 0; i < 4; ++i) {
                Color3f rgb;
                if constexpr (is_monochromatic_v<Spectrum>) {
                    rgb = spec.entry(i, 0).x();
                } else if constexpr (is_rgb_v<Spectrum>) {
                    rgb = spec.entry(i, 0);
                } else {
                    static_assert(is_spectral_v<Spectrum>);
                    /// Note: this assumes that sensor used sample_rgb_spectrum() to generate 'ray.wavelengths'
                    auto pdf = pdf_rgb_spectrum(ray.wavelengths);
                    UnpolarizedSpectrum _spec =
                        spec.entry(i, 0) * dr::select(pdf != 0.f, dr::rcp(pdf), 0.f);
                    rgb = spectrum_to_srgb(_spec, ray.wavelengths, active);
                }

                *aovs++ = rgb.r(); *aovs++ = rgb.g(); *aovs++ = rgb.b();
            }
        }

        return { spec, mask };
    }

    std::vector<std::string> aov_names() const override {
        std::vector<std::string> result = m_integrator->aov_names();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 3; ++j)
                result.insert(result.begin() + 3*i + j, "S" + std::to_string(i) + "." + ("RGB"[j]));
        return result;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("integrator", m_integrator.get(), +ParamFlags::Differentiable);
    }

    MI_DECLARE_CLASS()
private:
    ref<Base> m_integrator;
};

MI_IMPLEMENT_CLASS_VARIANT(StokesIntegrator, SamplingIntegrator)
MI_EXPORT_PLUGIN(StokesIntegrator, "Stokes integrator");
NAMESPACE_END(mitsuba)
