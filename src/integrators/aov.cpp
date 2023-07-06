#include <mitsuba/render/bsdf.h>
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


This integrator returns one or more AOVs (Arbitrary Output Variables) describing the visible
surfaces.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_diffuse_plain.jpg
   :caption: Scene rendered with a path tracer
.. subfigure:: ../../resources/data/docs/images/render/integrator_aov_depth.y.jpg
   :caption: Depth AOV
.. subfigure:: ../../resources/data/docs/images/render/integrator_aov_nn.jpg
   :caption: Normal AOV
.. subfigure:: ../../resources/data/docs/images/render/integrator_aov_position.jpg
   :caption: Position AOV
.. subfigend::
   :label: fig-diffuse

Here is an example on how to enable the *depth* and *shading normal* AOVs while still rendering the
image with a path tracer. The `RGBA` image produces by the path tracer will be stored in the
[:code:`my_image.R`, :code:`my_image.G`, :code:`my_image.B`, :code:`my_image.A`] channels of the EXR
output file.

.. tabs::
    .. code-tab:: xml

        <integrator type="aov">
            <string name="aovs" value="dd.y:depth,nn:sh_normal"/>
            <integrator type="path" name="my_image"/>
        </integrator>

    .. code-tab:: python

        'type': 'aov',
        'aovs': 'dd.y:depth,nn:sh_normal',
        'my_image': {
            'type': 'path',
        }

Currently, the following AOVs types are available:

    - :monosp:`albedo`: Albedo (diffuse reflectance) of the material.
    - :monosp:`depth`: Distance from the pinhole.
    - :monosp:`position`: World space position value.
    - :monosp:`uv`: UV coordinates.
    - :monosp:`geo_normal`: Geometric normal.
    - :monosp:`sh_normal`: Shading normal.
    - :monosp:`dp_du`, :monosp:`dp_dv`: Position partials wrt. the UV parameterization.
    - :monosp:`duv_dx`, :monosp:`duv_dy`: UV partials wrt. changes in screen-space.
    - :monosp:`prim_index`: Primitive index (e.g. triangle index in the mesh).
    - :monosp:`shape_index`: Shape index.
    - :monosp:`boundary_test`: Boundary test.

Note that integer-valued AOVs (e.g. :monosp:`prim_index`, :monosp:`shape_index`)
are meaningless whenever there is only partial pixel coverage or when using a
wide pixel reconstruction filter as it will result in fractional values.

The :monosp:`albedo` AOV will evaluate the diffuse reflectance
(\ref BSDF::eval_diffuse_reflectance) of the material. Note that depending on
the material, this value might only be an approximation.
 */

template <typename Float, typename Spectrum>
class AOVIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sampler, Medium, BSDFPtr)

    enum class Type {
        Albedo,
        Depth,
        Position,
        UV,
        GeometricNormal,
        ShadingNormal,
        BoundaryTest,
        dPdU,
        dPdV,
        dUVdx,
        dUVdy,
        PrimIndex,
        ShapeIndex,
        IntegratorRGBA
    };

    AOVIntegrator(const Properties &props) : Base(props) {
        std::vector<std::string> tokens = string::tokenize(props.string("aovs"));

        for (const std::string &token: tokens) {
            std::vector<std::string> item = string::tokenize(token, ":");

            if (item.size() != 2 || item[0].empty() || item[1].empty())
                Log(Warn, "Invalid AOV specification: require <name>:<type> pair");

            if (item[1] == "albedo") {
                m_aov_types.push_back(Type::Albedo);
                m_aov_names.push_back(item[0] + ".R");
                m_aov_names.push_back(item[0] + ".G");
                m_aov_names.push_back(item[0] + ".B");
            } else if (item[1] == "depth") {
                m_aov_types.push_back(Type::Depth);
                m_aov_names.push_back(item[0] + ".T");
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
            } else if (item[1] == "boundary_test") {
                m_aov_types.push_back(Type::BoundaryTest);
                m_aov_names.push_back(item[0]);
            } else if (item[1] == "dp_du") {
                m_aov_types.push_back(Type::dPdU);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "dp_dv") {
                m_aov_types.push_back(Type::dPdV);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "duv_dx") {
                m_aov_types.push_back(Type::dUVdx);
                m_aov_names.push_back(item[0] + ".U");
                m_aov_names.push_back(item[0] + ".V");
            } else if (item[1] == "duv_dy") {
                m_aov_types.push_back(Type::dUVdy);
                m_aov_names.push_back(item[0] + ".U");
                m_aov_names.push_back(item[0] + ".V");
            } else if (item[1] == "prim_index") {
                m_aov_types.push_back(Type::PrimIndex);
                m_aov_names.push_back(item[0] + ".I");
            } else if (item[1] == "shape_index") {
                m_aov_types.push_back(Type::ShapeIndex);
                m_aov_names.push_back(item[0] + ".I");
            } else {
                Throw("Invalid AOV type \"%s\"!", item[1]);
            }
        }

        for (auto &kv : props.objects()) {
            Base *integrator = dynamic_cast<Base *>(kv.second.get());
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
                                     const Medium *medium,
                                     Float *aovs,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        std::pair<Spectrum, Mask> result { 0.f, false };

        SurfaceInteraction3f si = scene->ray_intersect(
            ray, RayFlags::All | RayFlags::BoundaryTest, true, active);
        Mask valid_ray = active && si.is_valid();
        active &= si.is_valid();
        if (dr::none_or<false>(active))
        {
            for (size_t i = 0; i < m_aov_types.size(); ++i) {
                switch (m_aov_types[i]) {
                    case Type::Albedo: {
                            *aovs++ = 0;
                            *aovs++ = 0;
                            *aovs++ = 0;
                        }
                        break;
                    case Type::Depth:
                        *aovs++ = 0;
                        break;

                    case Type::Position:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::UV:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::GeometricNormal:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::ShadingNormal:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::BoundaryTest:
                        *aovs++ = 0;
                        break;

                    case Type::dPdU:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::dPdV:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::dUVdx:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::dUVdy:
                        *aovs++ = 0;
                        *aovs++ = 0;
                        break;

                    case Type::PrimIndex:
                        *aovs++ = 0;
                        break;

                    case Type::ShapeIndex:
                        *aovs++ = 0;
                        break;

                    case Type::IntegratorRGBA: {
                        *aovs++ = 0; *aovs++ = 0; *aovs++ = 0;
                        *aovs++ = 0;
                        }
                        break;
                }
            }
            return result;
        }
            
  
        dr::masked(si, !si.is_valid()) = dr::zeros<SurfaceInteraction3f>();
        size_t ctr = 0;

        BSDFContext ctx;
        BSDFPtr bsdf = si.bsdf(ray);
        

        auto spectrum_to_color3f = [](const Spectrum& spec, const Ray3f& ray, Mask active) {
            DRJIT_MARK_USED(active);
            UnpolarizedSpectrum spec_u = unpolarized_spectrum(spec);
            if constexpr (is_monochromatic_v<Spectrum>)
                return spec_u.x();
            else if constexpr (is_rgb_v<Spectrum>)
                return spec_u;
            else {
                static_assert(is_spectral_v<Spectrum>);
                /// Note: this assumes that sensor used sample_rgb_spectrum() to generate 'ray.wavelengths'
                auto pdf = pdf_rgb_spectrum(ray.wavelengths);
                spec_u *= dr::select(dr::neq(pdf, 0.f), dr::rcp(pdf), 0.f);
                return spectrum_to_srgb(spec_u, ray.wavelengths, active);
            }
        };

        Mask isGlass = active && has_flag(bsdf->flags(), BSDFFlags::Transmission);
        if (dr::any_or<true>(isGlass))
        {
            Vector3f wo = si.to_local(ray.d);
            auto [bsdf_val, bsdf_pdf, bsdf_sample, bsdf_weight]
                = bsdf->eval_pdf_sample(ctx, si, wo, 0.5, 0.5);
            Ray3f next_ray = si.spawn_ray(si.to_world(bsdf_sample.wo));
            SurfaceInteraction3f si2 = scene->ray_intersect(
            next_ray, +RayFlags::All, /* coherent = */ true, active);  
            for (size_t i = 0; i < m_aov_types.size(); ++i) {
                switch (m_aov_types[i]) {
                    case Type::Albedo: {
                            Color3f rgb(0.f);
                            if (dr::any_or<true>(si2.is_valid()))
                            {
                                Mask valid = active && si2.is_valid();
                                BSDFPtr m_bsdf = si2.bsdf(ray);

                                Spectrum spec =
                                    m_bsdf->eval_diffuse_reflectance(si2, valid);
                                dr::masked(rgb, valid) =
                                    spectrum_to_color3f(spec, ray, valid);
                            }

                            *aovs++ = rgb.r();
                            *aovs++ = rgb.g();
                            *aovs++ = rgb.b();
                        }
                        break;
                    case Type::Depth:
                        *aovs++ = dr::select(si2.is_valid(), si2.t, 0.f);
                        break;

                    case Type::Position:
                        *aovs++ = si2.p.x();
                        *aovs++ = si2.p.y();
                        *aovs++ = si2.p.z();
                        break;

                    case Type::UV:
                        *aovs++ = si2.uv.x();
                        *aovs++ = si2.uv.y();
                        break;

                    case Type::GeometricNormal:
                        *aovs++ = si2.n.x();
                        *aovs++ = si2.n.y();
                        *aovs++ = si2.n.z();
                        break;

                    case Type::ShadingNormal:
                        *aovs++ = si2.sh_frame.n.x();
                        *aovs++ = si2.sh_frame.n.y();
                        *aovs++ = si2.sh_frame.n.z();
                        break;

                    case Type::BoundaryTest:
                        *aovs++ = dr::select(si2.is_valid(), si2.boundary_test, 1.f);
                        break;

                    case Type::dPdU:
                        *aovs++ = si2.dp_du.x();
                        *aovs++ = si2.dp_du.y();
                        *aovs++ = si2.dp_du.z();
                        break;

                    case Type::dPdV:
                        *aovs++ = si2.dp_dv.x();
                        *aovs++ = si2.dp_dv.y();
                        *aovs++ = si2.dp_dv.z();
                        break;

                    case Type::dUVdx:
                        *aovs++ = si2.duv_dx.x();
                        *aovs++ = si2.duv_dx.y();
                        break;

                    case Type::dUVdy:
                        *aovs++ = si2.duv_dy.x();
                        *aovs++ = si2.duv_dy.y();
                        break;

                    case Type::PrimIndex:
                        *aovs++ = Float(si2.prim_index);
                        break;

                    case Type::ShapeIndex:
                        *aovs++ = Float(dr::reinterpret_array<UInt32>(si2.shape));
                        break;

                    case Type::IntegratorRGBA: {
                            std::pair<Spectrum, Mask> result_sub =
                                m_integrators[ctr].first->sample(scene, sampler, ray, medium, aovs, active);
                            aovs += m_integrators[ctr].second;
                            Color3f rgb =
                                spectrum_to_color3f(result_sub.first, ray, active);

                            *aovs++ = rgb.r(); *aovs++ = rgb.g(); *aovs++ = rgb.b();
                            *aovs++ = dr::select(result_sub.second, Float(1.f), Float(0.f));

                            if (ctr == 0)
                                result = result_sub;

                            ctr++;
                        }
                        break;
                }
            }
        }
        else
        {
            for (size_t i = 0; i < m_aov_types.size(); ++i) {
                switch (m_aov_types[i]) {
                    case Type::Albedo: {
                            Color3f rgb(0.f);
                            if (dr::any_or<true>(si.is_valid()))
                            {
                                Mask valid = active && si.is_valid();
                                BSDFPtr m_bsdf = si.bsdf(ray);

                                Spectrum spec =
                                    m_bsdf->eval_diffuse_reflectance(si, valid);
                                dr::masked(rgb, valid) =
                                    spectrum_to_color3f(spec, ray, valid);
                            }

                            *aovs++ = rgb.r();
                            *aovs++ = rgb.g();
                            *aovs++ = rgb.b();
                        }
                        break;
                    case Type::Depth:
                        *aovs++ = dr::select(si.is_valid(), si.t, 0.f);
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

                    case Type::BoundaryTest:
                        *aovs++ = dr::select(si.is_valid(), si.boundary_test, 1.f);
                        break;

                    case Type::dPdU:
                        *aovs++ = si.dp_du.x();
                        *aovs++ = si.dp_du.y();
                        *aovs++ = si.dp_du.z();
                        break;

                    case Type::dPdV:
                        *aovs++ = si.dp_dv.x();
                        *aovs++ = si.dp_dv.y();
                        *aovs++ = si.dp_dv.z();
                        break;

                    case Type::dUVdx:
                        *aovs++ = si.duv_dx.x();
                        *aovs++ = si.duv_dx.y();
                        break;

                    case Type::dUVdy:
                        *aovs++ = si.duv_dy.x();
                        *aovs++ = si.duv_dy.y();
                        break;

                    case Type::PrimIndex:
                        *aovs++ = Float(si.prim_index);
                        break;

                    case Type::ShapeIndex:
                        *aovs++ = Float(dr::reinterpret_array<UInt32>(si.shape));
                        break;

                    case Type::IntegratorRGBA: {
                            std::pair<Spectrum, Mask> result_sub =
                                m_integrators[ctr].first->sample(scene, sampler, ray, medium, aovs, active);
                            aovs += m_integrators[ctr].second;
                            Color3f rgb =
                                spectrum_to_color3f(result_sub.first, ray, active);

                            *aovs++ = rgb.r(); *aovs++ = rgb.g(); *aovs++ = rgb.b();
                            *aovs++ = dr::select(result_sub.second, Float(1.f), Float(0.f));

                            if (ctr == 0)
                                result = result_sub;

                            ctr++;
                        }
                        break;
                }
            }
        }
        

        return result;
    }

    std::vector<std::string> aov_names() const override {
        return m_aov_names;
    }

    void traverse(TraversalCallback *callback) override {
        for (size_t i = 0; i < m_integrators.size(); ++i)
            callback->put_object("integrator_" + std::to_string(i),
                                 m_integrators[i].first.get(),
                                 +ParamFlags::Differentiable);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AOVIntegrator[" << std::endl
            << "  aovs = " << m_aov_names << "," << std::endl
            << "  integrators = [" << std::endl;
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            oss << "    " << string::indent(m_integrators[i].first, 4);
            if (i + 1 < m_integrators.size())
                oss << ",";
            oss << std::endl;
        }
        oss << "  ]"<< std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    std::vector<Type> m_aov_types;
    std::vector<std::string> m_aov_names;
    std::vector<std::pair<ref<Base>, size_t>> m_integrators;
};

MI_IMPLEMENT_CLASS_VARIANT(AOVIntegrator, SamplingIntegrator)
MI_EXPORT_PLUGIN(AOVIntegrator, "AOV integrator");
NAMESPACE_END(mitsuba)
