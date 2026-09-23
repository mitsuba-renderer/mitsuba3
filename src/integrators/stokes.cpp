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

The film's base channels hold :math:`\mathbf{s}_0`, and the four entries follow
as channel groups :code:`S0` to :code:`S3` in the color space of the film. The
nested integrator cannot produce AOVs of its own. To add AOVs, nest the
:monosp:`stokes` integrator inside an :ref:`aov <integrator-aov>` integrator.

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
    MI_IMPORT_TYPES(Scene, Sensor, Sampler, Medium, Film)

    StokesIntegrator(const Properties &props) : Base(props) {
        if constexpr (!is_polarized_v<Spectrum>)
            Throw("This integrator should only be used in polarized mode!");
        for (auto &prop : props.objects()) {
            Base *integrator = prop.try_get<Base>();
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
                                     Sampler *sampler,
                                     const Ray3f &ray,
                                     const Medium *medium,
                                     Mask active) const override {
        return m_integrator->sample(scene, sampler, ray, medium, active);
    }

    Float *sample_channels(const Scene *scene,
                           const Sensor *sensor,
                           Sampler *sampler,
                           const Ray3f &ray,
                           const Spectrum &ray_weight,
                           const Medium *medium,
                           Float *out,
                           Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        if constexpr (is_polarized_v<Spectrum>) {
            auto [spec, valid] = m_integrator->sample(scene, sampler, ray, medium, active);

            // The Stokes vector that comes from the integrator is still aligned
            // with the implicit Stokes frame used for the ray direction. Apply
            // one last rotation here s.t. it aligns with the sensor's x-axis.
            Vector3f current_basis = mueller::stokes_basis(-ray.d);
            Vector3f vertical = sensor->world_transform(ray.time) * Vector3f(0.f, 1.f, 0.f);
            Vector3f target_basis = dr::cross(ray.d, vertical);
            spec = mueller::rotate_stokes_basis(-ray.d,
                                                 current_basis,
                                                 target_basis) * spec;

            // The base channels hold S0, followed by one group per component
            const Film *film = sensor->film();
            UnpolarizedSpectrum weight = unpolarized_spectrum(ray_weight);

            for (int i : { 0, 0, 1, 2, 3 }) {
                film->prepare_sample(spec.entry(i, 0) * weight, ray.wavelengths,
                                     out, valid, active);
                out += film->base_channels().size();
            }
            return out;
        } else {
            // Unreachable, since the constructor rejects unpolarized variants
            return Base::sample_channels(scene, sensor, sampler, ray, ray_weight,
                                         medium, out, active);
        }
    }

    std::vector<std::string> aov_names(const Film *film) const override {
        if (!m_integrator->aov_names(film).empty())
            Throw("The 'stokes' integrator does not support AOVs of its nested "
                  "integrator. Place the 'aov' integrator outside of it instead.");

        std::vector<std::string> result;
        for (int i = 0; i < 4; ++i)
            for (const std::string &name : film->base_channels())
                result.push_back("S" + std::to_string(i) + "." + name);
        return result;
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("integrator", m_integrator, ParamFlags::Differentiable);
    }

    MI_DECLARE_CLASS(StokesIntegrator)
private:
    ref<Base> m_integrator;

    MI_TRAVERSE_CB(Base, m_integrator)
};

MI_EXPORT_PLUGIN(StokesIntegrator)
NAMESPACE_END(mitsuba)
