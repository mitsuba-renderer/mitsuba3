#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)


/**!

.. _integrator-aov:

Arbitrary Output Variables integrator (:monosp:`aov`)
-----------------------------------------------------

.. pluginparameters::

 * - aovs
   - |string|
   - List of :monosp:`<name>:<type>` pairs denoting the enabled AOVs.
 * - (Nested plugin)
   - :paramtype:`integrator`
   - Sub-integrators (can have more than one) which will be sampled along the AOV integrator. Their
     respective output will be put into distinct images.


This integrator returns one or more AOVs (Arbitraty Output Variables) describing the visible
surfaces.

Here is an example on how to enable the *depth* and *shading normal* AOVs while still rendering the
image with a path tracer. The `RGBA` image produces by the path tracer will be stored in the
[:code:`my_image.R`, :code:`my_image.G`, :code:`my_image.B`, :code:`my_image.A`] channels of the EXR
output file.

.. code-block:: xml

    <integrator type="aov">
        <string name="aovs" value="dd.y:depth,nn:sh_normal"/>
        <integrator type="path" name="my_image"/>
    </integrator>


 */

template <typename Float, typename Spectrum>
class AOVIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(SamplingIntegrator)
    MTS_IMPORT_TYPES(Scene, Sampler, SamplingIntegrator)

    enum class Type {
        Depth,
        Position,
        UV,
        GeometricNormal,
        ShadingNormal,
        IntegratorRGBA
    };

    AOVIntegrator(const Properties &props) : Base(props) {
        std::vector<std::string> tokens = string::tokenize(props.string("aovs"));

        for (const std::string &token: tokens) {
            std::vector<std::string> item = string::tokenize(token, ":");

            if (item.size() != 2 || item[0].empty() || item[1].empty())
                Log(Warn, "Invalid AOV specification: require <name>:<type> pair");

            if (item[1] == "depth") {
                m_aov_types.push_back(Type::Depth);
                m_aov_names.push_back(item[0]);
            } else if (item[1] == "position") {
                m_aov_types.push_back(Type::Position);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "uv") {
                m_aov_types.push_back(Type::UV);
                m_aov_names.push_back(item[0] + ".U");
                m_aov_names.push_back(item[0] + ".V");
            } else if (item[1] == "geo_normal") {
                m_aov_types.push_back(Type::GeometricNormal);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "sh_normal") {
                m_aov_types.push_back(Type::ShadingNormal);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else {
                Throw("Invalid AOV type \"%s\"!", item[1]);
            }
        }

        for (auto &kv : props.objects()) {
            SamplingIntegrator *integrator = dynamic_cast<SamplingIntegrator *>(kv.second.get());
            if (!integrator)
                Throw("Child objects must be of type 'SamplingIntegrator'!");
            m_aov_types.push_back(Type::IntegratorRGBA);
            std::vector<std::string> aovs = integrator->aov_names();
            for (auto name: aovs)
                m_aov_names.push_back(kv.first + "." + name);
            m_integrators.push_back({ integrator, aovs.size() });
            m_aov_names.push_back(kv.first + ".R");
            m_aov_names.push_back(kv.first + ".G");
            m_aov_names.push_back(kv.first + ".B");
            m_aov_names.push_back(kv.first + ".A");
        }

        if (m_aov_names.empty())
            Log(Warn, "No AOVs were specified!");
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * sampler,
                                     const RayDifferential3f &ray,
                                     Float *aovs,
                                     Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        std::pair<Spectrum, Mask> result { 0.f, false };

        SurfaceInteraction3f si = scene->ray_intersect(ray, active);
        active = si.is_valid();
        si[!active] = zero<SurfaceInteraction3f>();
        size_t ctr = 0;

        for (size_t i = 0; i < m_aov_types.size(); ++i) {
            switch (m_aov_types[i]) {
                case Type::Depth:
                    *aovs++ = si.t;
                    break;

                case Type::Position:
                    *aovs++ = si.p.x();
                    *aovs++ = si.p.y();
                    *aovs++ = si.p.z();
                    break;

                case Type::UV:
                    *aovs++ = si.uv.x();
                    *aovs++ = si.uv.y();
                    break;

                case Type::GeometricNormal:
                    *aovs++ = si.n.x();
                    *aovs++ = si.n.y();
                    *aovs++ = si.n.z();
                    break;

                case Type::ShadingNormal:
                    *aovs++ = si.sh_frame.n.x();
                    *aovs++ = si.sh_frame.n.y();
                    *aovs++ = si.sh_frame.n.z();
                    break;

                case Type::IntegratorRGBA: {
                        std::pair<Spectrum, Mask> result_sub =
                            m_integrators[ctr].first->sample(scene, sampler, ray, aovs, active);
                        aovs += m_integrators[ctr].second;

                        UnpolarizedSpectrum spec_u = depolarize(result_sub.first);

                        Color3f rgb;
                        if constexpr (is_monochromatic_v<Spectrum>) {
                            rgb = spec_u.x();
                        } else if constexpr (is_rgb_v<Spectrum>) {
                            rgb = spec_u;
                        } else {
                            static_assert(is_spectral_v<Spectrum>);
                            /// Note: this assumes that sensor used sample_rgb_spectrum() to generate 'ray.wavelengths'
                            auto pdf = pdf_rgb_spectrum(ray.wavelengths);
                            spec_u *= select(neq(pdf, 0.f), rcp(pdf), 0.f);
                            rgb = xyz_to_srgb(spectrum_to_xyz(spec_u, ray.wavelengths, active));
                        }

                        *aovs++ = rgb.r(); *aovs++ = rgb.g(); *aovs++ = rgb.b();
                        *aovs++ = select(result_sub.second, Float(1.f), Float(0.f));

                        if (ctr == 0)
                            result = result_sub;

                        ctr++;
                    }
                    break;
            }
        }

        return result;
    }

    std::vector<std::string> aov_names() const override {
        return m_aov_names;
    }

    void traverse(TraversalCallback *callback) override {
        for (size_t i = 0; i < m_integrators.size(); ++i)
            callback->put_object("integrator_" + std::to_string(i), m_integrators[i].first.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Scene[" << std::endl
            << "  aovs = " << m_aov_names << "," << std::endl
            << "  integrators = [" << std::endl;
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            oss << "    " << string::indent(m_integrators[i].first->to_string(), 4);
            if (i + 1 < m_integrators.size())
                oss << ",";
            oss << std::endl;
        }
        oss << "  ]"<< std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    std::vector<Type> m_aov_types;
    std::vector<std::string> m_aov_names;
    std::vector<std::pair<ref<SamplingIntegrator>, size_t>> m_integrators;
};

MTS_IMPLEMENT_CLASS_VARIANT(AOVIntegrator, SamplingIntegrator)
MTS_EXPORT_PLUGIN(AOVIntegrator, "AOV integrator");
NAMESPACE_END(mitsuba)
