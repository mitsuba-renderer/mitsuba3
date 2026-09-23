#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sensor.h>

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
image with a path tracer. The image produced by the path tracer occupies the base channels of the
film (:code:`R`, :code:`G`, :code:`B`), and the AOVs follow in the order in which they were
specified. When several sub-integrators are given, the first one occupies the base channels and each
of the others receives a group of channels named after it, e.g. [:code:`my_image2.R`,
:code:`my_image2.G`, :code:`my_image2.B`].

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

    - :monosp:`albedo`: Albedo (directional reflectance and transmittance) of the material.
    - :monosp:`diffuse_albedo`, :monosp:`specular_reflectance`,
      :monosp:`specular_transmittance`: The individual parts of the albedo.
    - :monosp:`roughness`: Roughness of the specular lobes.
    - :monosp:`depth`: Distance from the pinhole.
    - :monosp:`position`: World space position value.
    - :monosp:`uv`: UV coordinates.
    - :monosp:`geo_normal`: Geometric normal.
    - :monosp:`sh_normal`: Shading normal.
    - :monosp:`dp_du`, :monosp:`dp_dv`: Position partials wrt. the UV parameterization.
    - :monosp:`prim_index`: Primitive index (e.g. triangle index in the mesh).
    - :monosp:`shape_index`: Shape index.

Note that integer-valued AOVs (e.g. :monosp:`prim_index`, :monosp:`shape_index`)
are meaningless whenever there is only partial pixel coverage or when using a
wide pixel reconstruction filter as it will result in fractional values.

The :monosp:`albedo` AOV sums the diffuse, specular reflection, and specular
transmission albedos reported by `BSDF::eval_features()`, which also provides
the other material-related AOVs. Note that depending on the material, these
values might only be approximations.
 */

template <typename Float, typename Spectrum>
class AOVIntegratorImpl final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sensor, Sampler, Medium, Film, ShapePtr)

    enum class AOVType {
        Albedo,
        DiffuseAlbedo,
        SpecularReflectance,
        SpecularTransmittance,
        Roughness,
        Depth,
        Position,
        UV,
        GeometricNormal,
        ShadingNormal,
        dPdU,
        dPdV,
        PrimIndex,
        ShapeIndex,
    };

    AOVIntegratorImpl(std::string_view aovs_spec) : Base(Properties()) {
        std::vector<std::string> tokens = string::tokenize(aovs_spec);

        for (const std::string &token: tokens) {
            std::vector<std::string> item = string::tokenize(token, ":");

            if (item.size() != 2 || item[0].empty() || item[1].empty())
                Log(Warn, "Invalid AOV specification: require <name>:<type> pair");

            if (item[1] == "albedo") {
                m_aov_types.push_back(AOVType::Albedo);
                m_aov_names.push_back(item[0] + ".R");
                m_aov_names.push_back(item[0] + ".G");
                m_aov_names.push_back(item[0] + ".B");
            } else if (item[1] == "diffuse_albedo") {
                m_aov_types.push_back(AOVType::DiffuseAlbedo);
                m_aov_names.push_back(item[0] + ".R");
                m_aov_names.push_back(item[0] + ".G");
                m_aov_names.push_back(item[0] + ".B");
            } else if (item[1] == "specular_reflectance") {
                m_aov_types.push_back(AOVType::SpecularReflectance);
                m_aov_names.push_back(item[0] + ".R");
                m_aov_names.push_back(item[0] + ".G");
                m_aov_names.push_back(item[0] + ".B");
            } else if (item[1] == "specular_transmittance") {
                m_aov_types.push_back(AOVType::SpecularTransmittance);
                m_aov_names.push_back(item[0] + ".R");
                m_aov_names.push_back(item[0] + ".G");
                m_aov_names.push_back(item[0] + ".B");
            } else if (item[1] == "roughness") {
                m_aov_types.push_back(AOVType::Roughness);
                m_aov_names.push_back(item[0] + ".Y");
            } else if (item[1] == "depth") {
                m_aov_types.push_back(AOVType::Depth);
                m_aov_names.push_back(item[0] + ".T");
            } else if (item[1] == "position") {
                m_aov_types.push_back(AOVType::Position);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "uv") {
                m_aov_types.push_back(AOVType::UV);
                m_aov_names.push_back(item[0] + ".U");
                m_aov_names.push_back(item[0] + ".V");
            } else if (item[1] == "geo_normal") {
                m_aov_types.push_back(AOVType::GeometricNormal);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "sh_normal") {
                m_aov_types.push_back(AOVType::ShadingNormal);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "dp_du") {
                m_aov_types.push_back(AOVType::dPdU);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "dp_dv") {
                m_aov_types.push_back(AOVType::dPdV);
                m_aov_names.push_back(item[0] + ".X");
                m_aov_names.push_back(item[0] + ".Y");
                m_aov_names.push_back(item[0] + ".Z");
            } else if (item[1] == "prim_index") {
                m_aov_types.push_back(AOVType::PrimIndex);
                m_aov_names.push_back(item[0] + ".I");
            } else if (item[1] == "shape_index") {
                m_aov_types.push_back(AOVType::ShapeIndex);
                m_aov_names.push_back(item[0] + ".I");
            } else {
                Throw("Invalid AOV type \"%s\"!", item[1]);
            }
        }

        for (AOVType type : m_aov_types)
            m_needs_features |= type == AOVType::Albedo ||
                                type == AOVType::DiffuseAlbedo ||
                                type == AOVType::SpecularReflectance ||
                                type == AOVType::SpecularTransmittance ||
                                type == AOVType::Roughness ||
                                type == AOVType::ShadingNormal;
    }

    std::pair<Spectrum, Mask> sample(const Scene * /* scene */,
                                     Sampler * /* sampler */,
                                     const Ray3f & /* ray */,
                                     const Medium * /* medium */,
                                     Mask /* active */) const override {
        return { 0.f, false };
    }

    Float *sample_channels(const Scene *scene,
                           const Sensor *sensor,
                           Sampler * /* sampler */,
                           const Ray3f &ray,
                           const Spectrum &ray_weight,
                           const Medium * /* medium */,
                           Float *out,
                           Mask active) const override {
        // The base channels hold no radiance
        const Film *film = sensor->film();
        film->prepare_sample(0.f, ray.wavelengths, out, false, active);
        out += film->base_channels().size();
        return sample_aovs(scene, ray, ray_weight, out, active);
    }

    /// Write the AOVs of a camera ray and return the end of the written range
    Float *sample_aovs(const Scene *scene,
                       const Ray3f &ray,
                       const Spectrum &ray_weight,
                       Float *aovs,
                       Mask active) const {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        SurfaceInteraction3f si =
            scene->ray_intersect(ray, +RayFlags::Default, /* coherent = */ true,
                                 +RayMask::Primary, active);
        dr::masked(si, !si.is_valid()) = dr::zeros<SurfaceInteraction3f>();

        // All material-related AOVs are answered by this record
        Mask valid = active && si.is_valid();
        BSDFFeatures3f features = dr::zeros<BSDFFeatures3f>();
        if (m_needs_features && dr::any_or<true>(si.is_valid()))
            features = si.bsdf()->eval_features(si, valid);

        // The ray weight accounts for the sampled wavelengths in spectral variants
        auto spectrum_to_color3f = [&](const Spectrum &spec, Mask active) {
            DRJIT_MARK_USED(active);
            UnpolarizedSpectrum spec_u = unpolarized_spectrum(spec) *
                                         unpolarized_spectrum(ray_weight);
            if constexpr (is_monochromatic_v<Spectrum>)
                return Color3f(spec_u.x());
            else if constexpr (is_rgb_v<Spectrum>)
                return Color3f(spec_u);
            else
                return spectrum_to_srgb(spec_u, ray.wavelengths, active);
        };

        auto write_color = [&](const UnpolarizedSpectrum &spec) {
            Color3f rgb(0.f);
            dr::masked(rgb, valid) = spectrum_to_color3f(spec, valid);
            *aovs++ = rgb.r();
            *aovs++ = rgb.g();
            *aovs++ = rgb.b();
        };

        for (size_t i = 0; i < m_aov_types.size(); ++i) {
            switch (m_aov_types[i]) {
                case AOVType::Albedo:
                    write_color(features.diffuse_albedo +
                                features.specular_reflectance +
                                features.specular_transmittance);
                    break;

                case AOVType::DiffuseAlbedo:
                    write_color(features.diffuse_albedo);
                    break;

                case AOVType::SpecularReflectance:
                    write_color(features.specular_reflectance);
                    break;

                case AOVType::SpecularTransmittance:
                    write_color(features.specular_transmittance);
                    break;

                case AOVType::Roughness:
                    *aovs++ = dr::select(valid, features.roughness, 0.f);
                    break;

                case AOVType::Depth:
                    *aovs++ = dr::select(si.is_valid(), si.t, 0.f);
                    break;

                case AOVType::Position:
                    *aovs++ = si.p.x();
                    *aovs++ = si.p.y();
                    *aovs++ = si.p.z();
                    break;

                case AOVType::UV:
                    *aovs++ = si.uv.x();
                    *aovs++ = si.uv.y();
                    break;

                case AOVType::GeometricNormal:
                    *aovs++ = si.n.x();
                    *aovs++ = si.n.y();
                    *aovs++ = si.n.z();
                    break;

                case AOVType::ShadingNormal: {
                        const Normal3f &n = features.sh_frame.n;
                        *aovs++ = n.x();
                        *aovs++ = n.y();
                        *aovs++ = n.z();
                    }
                    break;

                case AOVType::dPdU:
                    *aovs++ = si.dp_du.x();
                    *aovs++ = si.dp_du.y();
                    *aovs++ = si.dp_du.z();
                    break;

                case AOVType::dPdV:
                    *aovs++ = si.dp_dv.x();
                    *aovs++ = si.dp_dv.y();
                    *aovs++ = si.dp_dv.z();
                    break;

                case AOVType::PrimIndex:
                    *aovs++ = Float(si.prim_index);
                    break;

                case AOVType::ShapeIndex:
                    if constexpr (!dr::is_jit_v<Float>) {
                        ShapePtr target = si.instance_index != 0
                            ? scene->instance(si.instance_index - 1)
                            : si.shape;
                        *aovs++ = Float(scene->shape_index(target));
                    } else {
                        *aovs++ = Float(dr::reinterpret_array<UInt32>(si.shape));
                    }
                    break;
            }
        }

        return aovs;
    }

    std::vector<std::string> aov_names(const Film * /* film */) const override {
        return m_aov_names;
    }

    bool empty() const { return m_aov_names.empty(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AOVIntegratorImpl[" << std::endl
            << "  aovs = " << m_aov_names << std::endl
            << "]";
        return oss.str();
    }

private:
    std::vector<AOVType> m_aov_types;
    std::vector<std::string> m_aov_names;
    bool m_needs_features = false;
};

/**
 * Combines the images of the nested integrators and the built-in AOV pass
 * into a single film.
 *
 * The image of the first source occupies the film's base channels, every
 * further nested integrator contributes its base channels under its own name,
 * and the built-in AOVs come last.
 */
template <typename Float, typename Spectrum>
class AOVIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sensor, Sampler, Medium, Film)
    using Impl = AOVIntegratorImpl<Float, Spectrum>;

    AOVIntegrator(const Properties &props) : Base(props) {
        for (auto &prop : props.objects()) {
            Base *integrator = prop.try_get<Base>();
            if (!integrator)
                Throw("Child objects must be of type 'SamplingIntegrator'!");
            m_integrators.push_back(integrator);
            m_names.push_back(std::string(prop.name()));
        }

        std::string_view aovs = props.get<std::string_view>("aovs");
        if (!aovs.empty()) {
            ref<Impl> impl = new Impl(aovs);
            if (!impl->empty()) {
                m_impl = impl;
                m_integrators.push_back(impl.get());
                m_names.push_back("");
            }
        }

        if (m_integrators.empty())
            Throw("No sub-integrators or AOVs were specified!");

        if (!m_impl && m_integrators.size() == 1)
            Log(Warn, "No AOVs were specified!");
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
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            if (skips_base(i))
                out = m_impl->sample_aovs(scene, ray, ray_weight, out, active);
            else
                out = m_integrators[i]->sample_channels(
                    scene, sensor, sampler, ray, ray_weight, medium, out, active);
        }
        return out;
    }

    TensorXf render_forward(Scene *scene,
                            void *params,
                            Sensor *sensor,
                            UInt32 seed = 0,
                            uint32_t spp = 0) override {
        const Film *film = sensor->film();
        size_t base = film->base_channels().size();

        std::vector<TensorXf> grads;
        std::vector<size_t> skip;
        size_t n_channels = 0;
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            grads.push_back(m_integrators[i]->render_forward(scene, params, sensor, seed, spp));
            skip.push_back(skips_base(i) ? base : 0);
            n_channels += grads[i].shape(2) - skip[i];
        }

        TensorXf result = zeros_like(grads[0], n_channels);
        size_t offset = 0;
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            size_t width = grads[i].shape(2) - skip[i];
            copy_channels(grads[i], skip[i], result, offset, width);
            offset += width;
        }
        return result;
    }

    void render_backward(Scene *scene,
                         void *params,
                         const TensorXf &grad_in,
                         Sensor *sensor,
                         UInt32 seed = 0,
                         uint32_t spp = 0) override {
        const Film *film = sensor->film();
        size_t base = film->base_channels().size(), offset = 0;

        for (size_t i = 0; i < m_integrators.size(); ++i) {
            // The gradient image of a source has its base channels followed by its AOVs
            Base *integrator = m_integrators[i];
            size_t n_channels = base + integrator->aov_names(film).size(),
                   skip = skips_base(i) ? base : 0,
                   width = n_channels - skip;
            TensorXf grad = zeros_like(grad_in, n_channels);
            copy_channels(grad_in, offset, grad, skip, width);
            integrator->render_backward(scene, params, grad, sensor, seed, spp);
            offset += width;
        }
    }

    std::vector<std::string> aov_names(const Film *film) const override {
        std::vector<std::string> result;
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            // Nested integrators after the first contribute their base
            // channels under their own name
            bool own = m_integrators[i].get() == m_impl.get();
            if (i > 0 && !own)
                for (const std::string &name : film->base_channels())
                    result.push_back(m_names[i] + "." + name);

            for (const std::string &name : m_integrators[i]->aov_names(film))
                result.push_back(own ? name : m_names[i] + "." + name);
        }
        return result;
    }

    void traverse(TraversalCallback *cb) override {
        for (size_t i = 0; i < m_integrators.size(); ++i)
            cb->put("integrator_" + std::to_string(i),
                                 m_integrators[i],
                                 ParamFlags::Differentiable);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AOVIntegrator[" << std::endl
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

    MI_DECLARE_CLASS(AOVIntegrator)
protected:
    /// The first source fills the film's base channels. Specify whether source
    /// ``i`` is a later built-in AOV pass, whose zero-valued base channels are dropped.
    bool skips_base(size_t i) const {
        return i > 0 && m_integrators[i].get() == m_impl.get();
    }

    /// Zero-valued image with the resolution of 'ref' and the given channel count
    static TensorXf zeros_like(const TensorXf &ref, size_t channels) {
        size_t shape[3] = { ref.shape(0), ref.shape(1), channels };
        return TensorXf(dr::zeros<typename TensorXf::Array>(shape[0] * shape[1] * channels),
                        3, shape);
    }

    /// Copy a range of channels from one image into another
    static void copy_channels(const TensorXf &src, size_t src_offset,
                              TensorXf &dst, size_t dst_offset, size_t count) {
        using Index = DynamicBuffer<UInt32>;
        using Array = typename TensorXf::Array;

        if (count == 0)
            return;

        uint32_t pixel_count = (uint32_t) (src.shape(0) * src.shape(1)),
                 src_ch      = (uint32_t) src.shape(2),
                 dst_ch      = (uint32_t) dst.shape(2);

        Index idx     = dr::arange<Index>(pixel_count * (uint32_t) count),
              pixel   = idx / (uint32_t) count,
              channel = dr::fmadd(pixel, uint32_t(-(int) count), idx);

        dr::scatter(dst.array(),
                    dr::gather<Array>(src.array(), dr::fmadd(pixel, src_ch, channel + (uint32_t) src_offset)),
                    dr::fmadd(pixel, dst_ch, channel + (uint32_t) dst_offset));
    }

private:
    std::vector<ref<Base>> m_integrators;
    std::vector<std::string> m_names;
    ref<Impl> m_impl;

    MI_TRAVERSE_CB(Base, m_integrators)
};

MI_EXPORT_PLUGIN(AOVIntegrator)
NAMESPACE_END(mitsuba)

