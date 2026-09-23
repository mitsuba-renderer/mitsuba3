#include <mitsuba/render/film.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)


/**!

.. _integrator-moment:

Moment integrator (:monosp:`moment`)
-----------------------------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - :paramtype:`integrator`
   - Sub-integrators (can have more than one) which will be sampled along the moment integrator.

This integrator records the first and second moments of the samples of the
nested integrators, which is useful to estimate the variance of a rendering.

For each image channel named ``X``, this integrator will contribute an additional channel
named ``m2.X`` storing the second moment.

.. tabs::
    .. code-tab:: xml

        <integrator type="moment">
            <integrator type="path"/>
        </integrator>

    .. code-tab:: python

        'type': 'moment',
        'nested': {
            'type': 'path',
        }

 */

template <typename Float, typename Spectrum>
class MomentIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sensor, Sampler, Medium, Film)

    MomentIntegrator(const Properties &props) : Base(props) {
        for (auto &prop : props.objects()) {
            Base *integrator = prop.try_get<Base>();
            if (!integrator)
                Throw("Child objects must be of type 'SamplingIntegrator'!");
            m_integrators.push_back(integrator);
            m_names.push_back(std::string(prop.name()));
        }

        if (m_integrators.empty())
            Throw("Must specify at least one sub-integrator!");
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const Ray3f &ray,
                                     const Medium *medium,
                                     Mask active) const override {
        return m_integrators[0]->sample(scene, sampler, ray, medium, active);
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

        Float *begin = out;
        for (auto &integrator : m_integrators)
            out = integrator->sample_channels(scene, sensor, sampler, ray,
                                              ray_weight, medium, out, active);

        // Second moments of all channels written above
        size_t n = (size_t) (out - begin);
        for (size_t i = 0; i < n; ++i)
            out[i] = dr::square(begin[i]);

        return out + n;
    }

    std::vector<std::string> aov_names(const Film *film) const override {
        const std::vector<std::string> &base_names = film->base_channels();
        std::vector<std::string> result;

        for (size_t i = 0; i < m_integrators.size(); ++i) {
            if (i > 0)
                for (const std::string &name : base_names)
                    result.push_back(m_names[i] + "." + name);
            for (const std::string &name : m_integrators[i]->aov_names(film))
                result.push_back(m_names[i] + "." + name);
        }

        std::vector<std::string> m2;
        for (const std::string &name : base_names)
            m2.push_back("m2." + name);
        for (const std::string &name : result)
            m2.push_back("m2." + name);

        result.insert(result.end(), m2.begin(), m2.end());
        return result;
    }

    void traverse(TraversalCallback *cb) override {
        for (size_t i = 0; i < m_integrators.size(); ++i)
            cb->put("integrator_" + std::to_string(i),
                                 m_integrators[i].get(),
                                 ParamFlags::Differentiable);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MomentIntegrator[" << std::endl
            << "  integrators = [" << std::endl;
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            oss << "    " << string::indent(m_integrators[i], 4);
            if (i + 1 < m_integrators.size())
                oss << ",";
            oss << std::endl;
        }
        oss << "  ]"<< std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(MomentIntegrator)
private:
    std::vector<ref<Base>> m_integrators;
    std::vector<std::string> m_names;

    MI_TRAVERSE_CB(Base, m_integrators)
};

MI_EXPORT_PLUGIN(MomentIntegrator)
NAMESPACE_END(mitsuba)
