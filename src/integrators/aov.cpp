#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sensor.h>
#include <unordered_map>

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

    - :monosp:`albedo`: Albedo (diffuse reflectance) of the material.
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

The :monosp:`albedo` AOV will evaluate the diffuse reflectance
(`BSDF::eval_features()`) of the material. Note that depending on the material,
this value might only be an approximation.
 */

template <typename Float, typename Spectrum>
class AOVIntegratorImpl final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Shape, Sensor, Sampler, Medium, BSDFPtr, ShapePtr)

    enum class AOVType {
        Albedo,
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
                m_has_shape_index_aov = true;
            } else {
                Throw("Invalid AOV type \"%s\"!", item[1]);
            }
        }

        for (AOVType type : m_aov_types)
            m_needs_features |= type == AOVType::Albedo ||
                                type == AOVType::ShadingNormal;
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * /*sampler*/,
                                     const Ray3f &ray,
                                     const Medium * /*medium*/,
                                     Float *aovs,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        std::pair<Spectrum, Mask> result { 0.f, false };
        SurfaceInteraction3f si =
            scene->ray_intersect(ray, +RayFlags::Default, /* coherent = */ true,
                                 +RayMask::Primary, active);
        dr::masked(si, !si.is_valid()) = dr::zeros<SurfaceInteraction3f>();

        // The albedo and shading normal AOVs are both answered by this record
        BSDFFeatures3f features = dr::zeros<BSDFFeatures3f>();
        if (m_needs_features && dr::any_or<true>(si.is_valid()))
            features = si.bsdf()->eval_features(si, active && si.is_valid());

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
                spec_u *= dr::select(pdf != 0.f, dr::rcp(pdf), 0.f);
                return spectrum_to_srgb(spec_u, ray.wavelengths, active);
            }
        };

        for (size_t i = 0; i < m_aov_types.size(); ++i) {
            switch (m_aov_types[i]) {
                case AOVType::Albedo: {
                        Mask valid = active && si.is_valid();
                        Color3f rgb(0.f);
                        dr::masked(rgb, valid) =
                            spectrum_to_color3f(features.albedo, ray, valid);

                        *aovs++ = rgb.r();
                        *aovs++ = rgb.g();
                        *aovs++ = rgb.b();
                    }
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

                        auto it = m_shape_to_idx.find(target);
                        if (it == m_shape_to_idx.end())
                            *aovs++ = 0;
                        else
                            *aovs++ = Float(it->second);
                    } else {
                        *aovs++ = Float(dr::reinterpret_array<UInt32>(si.shape));
                    }
                    break;
            }
        }

        return result;
    }

    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    UInt32 seed,
                    uint32_t spp,
                    bool develop,
                    bool evaluate,
                    bool profile) override {

        // Prepare shape indexing data structure for scalar variants
        if constexpr (!dr::is_jit_v<Float>) {
            if (m_has_shape_index_aov) {
                m_shape_to_idx.clear();
                size_t counter = 1;
                for (const ref<Shape>& shape : scene->shapes())
                    m_shape_to_idx[shape.get()] = (uint32_t) counter++;
            }
        }
        return Base::render(scene, sensor, seed, spp, develop, evaluate,
                            profile);
    }

    std::vector<std::string> aov_names() const override {
        return m_aov_names;
    }

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
    bool m_has_shape_index_aov = false;
    bool m_needs_features = false;
    std::unordered_map<const Shape*, uint32_t> m_shape_to_idx;
};

/**
 * Combines the images of the nested integrators and the built-in AOV pass
 * into a single film.
 *
 * Each source renders on its own into the shared film. The developed image
 * of the first source occupies the film's base channels, every further nested
 * integrator contributes its base channels under its own name, and the
 * built-in AOVs come last. This layout is a list of segments that
 * rendering, forward and backward differentiation all walk in the same way.
 */
template <typename Float, typename Spectrum>
class AOVIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Sensor, Sampler, Medium, Film, ImageBlock)
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
            if (!impl->aov_names().empty()) {
                m_impl = impl;
                m_integrators.push_back(impl.get());
                m_names.push_back("");
            }
        }

        if (m_integrators.empty())
            Throw("No sub-integrators or AOVs were specified!");

        // The channel names of nested integrators depend on the film's base
        // channels, which are unknown here. Assume an RGB film until a render
        // reveals the actual one.
        m_base_names = { "R", "G", "B" };

        if (aov_names().empty())
            Log(Warn, "No AOVs were specified!");
    }

    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    UInt32 seed,
                    uint32_t spp,
                    bool develop,
                    bool evaluate,
                    bool profile) override {
        Film *film = sensor->film();
        m_base_names = film->base_channels();

        // Render each source and keep the raw storage that it leaves in the film
        std::vector<TensorXf> raw;
        for (auto &integrator : m_integrators) {
            integrator->render(scene, sensor, seed, spp, false, evaluate, profile);
            raw.push_back(film->storage());
        }

        // Assemble the combined image, reusing the sample weight of the first source
        size_t n_channels = film->prepare(aov_names());
        TensorXf block = concat(raw, n_channels);
        copy_channels(raw[0], raw[0].shape(2) - 1, block, n_channels - 1, 1);

        ref<ImageBlock> image_block = new ImageBlock(block, film->crop_offset());
        film->put_block(image_block);

        TensorXf result;
        if (develop) {
            result = film->develop();
            dr::schedule(result);
        } else {
            dr::schedule(film->storage());
        }

        if (evaluate)
            dr::eval();

        return result;
    }

    TensorXf render_forward(Scene *scene,
                            void *params,
                            Sensor *sensor,
                            UInt32 seed = 0,
                            uint32_t spp = 0) override {
        m_base_names = sensor->film()->base_channels();

        std::vector<TensorXf> grads;
        for (auto &integrator : m_integrators)
            grads.push_back(integrator->render_forward(scene, params, sensor, seed, spp));

        return concat(grads, m_base_names.size() + aov_names().size());
    }

    void render_backward(Scene *scene,
                         void *params,
                         const TensorXf &grad_in,
                         Sensor *sensor,
                         UInt32 seed = 0,
                         uint32_t spp = 0) override {
        m_base_names = sensor->film()->base_channels();

        size_t offset = 0;
        for (const Segment &seg : layout()) {
            // The gradient image of a source has its base channels followed by its AOVs
            Base *integrator = m_integrators[seg.source];
            size_t n_channels = m_base_names.size() + integrator->aov_names().size();
            TensorXf grad = zeros_like(grad_in, n_channels);
            copy_channels(grad_in, offset, grad, seg.offset, seg.width);
            integrator->render_backward(scene, params, grad, sensor, seed, spp);
            offset += seg.width;
        }
    }

    std::vector<std::string> aov_names() const override {
        std::vector<std::string> result;
        for (const Segment &seg : layout())
            result.insert(result.end(), seg.names.begin(), seg.names.end());
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
            << "  aovs = " << aov_names() << "," << std::endl
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
    /// A range of channels that one source contributes to the developed image
    struct Segment {
        size_t source;  // index into m_integrators
        size_t offset;  // first channel within the source's developed image
        size_t width;
        std::vector<std::string> names;  // AOV names, excluding the film's base channels
    };

    std::vector<Segment> layout() const {
        std::vector<Segment> result;
        size_t base = m_base_names.size();

        for (size_t i = 0; i < m_integrators.size(); ++i) {
            bool own = m_integrators[i].get() == m_impl.get();
            Segment seg { i, 0, 0, {} };

            // The first source fills the film's base channels. Further nested
            // integrators contribute theirs under their name, while the
            // built-in AOV pass leaves its base channels empty and skips them.
            if (i == 0 || !own)
                seg.width = base;
            else
                seg.offset = base;

            if (i > 0 && !own)
                for (const std::string &name : m_base_names)
                    seg.names.push_back(m_names[i] + "." + name);

            for (const std::string &name : m_integrators[i]->aov_names()) {
                seg.names.push_back(own ? name : m_names[i] + "." + name);
                seg.width++;
            }

            result.push_back(std::move(seg));
        }

        return result;
    }

    /// Assemble the segments of the given source images into one image
    TensorXf concat(const std::vector<TensorXf> &images, size_t n_channels) const {
        TensorXf result = zeros_like(images[0], n_channels);
        size_t offset = 0;
        for (const Segment &seg : layout()) {
            copy_channels(images[seg.source], seg.offset, result, offset, seg.width);
            offset += seg.width;
        }
        return result;
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
    std::vector<std::string> m_base_names;

    MI_TRAVERSE_CB(Base, m_integrators)
};

MI_EXPORT_PLUGIN(AOVIntegrator)
NAMESPACE_END(mitsuba)

