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
    MI_IMPORT_TYPES(Scene, Shape, Sensor, Sampler, Medium, BSDFPtr, ShapePtr)

    enum class Type {
        Albedo,
        Depth,
        Position,
        UV,
        GeometricNormal,
        ShadingNormal,
        dPdU,
        dPdV,
        dUVdx,
        dUVdy,
        PrimIndex,
        ShapeIndex,
        IntegratorRGBA
    };

    AOVIntegrator(const Properties &props) : Base(props),
        m_integrator_aovs_count(0) {
        std::vector<std::string> tokens = string::tokenize(props.string("aovs"));

        for (auto &kv : props.objects()) {
            Base *integrator = dynamic_cast<Base *>(kv.second.get());
            if (!integrator)
                Throw("Child objects must be of type 'SamplingIntegrator'!");

            m_integrators.push_back(integrator);
            m_aov_types.push_back(Type::IntegratorRGBA);
            m_aov_names.push_back(kv.first + ".R");
            m_aov_names.push_back(kv.first + ".G");
            m_aov_names.push_back(kv.first + ".B");
            m_aov_names.push_back(kv.first + ".A");
            m_integrator_aovs_count+= 4;
        }

        for (auto &kv : props.objects()) {
            Base *integrator = dynamic_cast<Base *>(kv.second.get());
            std::vector<std::string> aovs = integrator->aov_names();
            for (auto name: aovs)
                m_aov_names.push_back(kv.first + "." + name);
        }

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

        if (m_aov_names.empty())
            Log(Warn, "No AOVs were specified!");
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * sampler,
                                     const RayDifferential3f &ray,
                                     const Medium * medium,
                                     Float *_aovs,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        std::pair<Spectrum, Mask> result { 0.f, false };

        SurfaceInteraction3f si =
            scene->ray_intersect(ray, (uint32_t) RayFlags::All, true, active);
        dr::masked(si, !si.is_valid()) = dr::zeros<SurfaceInteraction3f>();

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

        // Shape indexing data structure for scalar variants
        std::unordered_map<const Shape*, uint32_t> shape_to_idx{};
        std::vector<ref<Shape>> shapes = scene->shapes();
        size_t counter = 1; // 0 reserved for background
        for (const ref<Shape>& shape: shapes)
            shape_to_idx[shape.get()] = (uint32_t) counter++;

        // We want to pack the channels such that base_channels and inner-integrator
        // RGBA channels are contiguous
        Float* aovs_rgba_integrator = _aovs;
        Float* aovs = _aovs + m_integrator_aovs_count;

        size_t inner_idx = 0;
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
                    si.compute_uv_partials(ray);
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
                    if constexpr (!dr::is_jit_v<Float>) {
                        ShapePtr target = si.instance;
                        if (!target)
                            target = si.shape;

                        auto it = shape_to_idx.find(target);
                        if (it == shape_to_idx.end())
                            *aovs++ = 0;
                        else
                            *aovs++ = Float(it->second);
                    } else {
                        *aovs++ = Float(dr::reinterpret_array<UInt32>(si.shape));
                    }
                    break;

                case Type::IntegratorRGBA: {
                    auto [inner_spec, inner_mask] 
                        = m_integrators[inner_idx]->sample(scene, sampler, ray, medium, aovs, active);
                    dr::disable_grad(inner_spec);

                    Color3f rgb = spectrum_to_color3f(inner_spec, ray, active);

                    aovs += m_integrators[inner_idx]->aov_names().size();
                    *aovs_rgba_integrator++ = rgb.r();
                    *aovs_rgba_integrator++ = rgb.g();
                    *aovs_rgba_integrator++ = rgb.b();
                    *aovs_rgba_integrator++ = dr::select(inner_mask, Float(1.f), Float(0.f));
                    if (inner_idx == 0)
                        result = {inner_spec, inner_mask};

                    inner_idx++;
                } break;
            }
        }

        return result;
    }

    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    uint32_t seed,
                    uint32_t spp,
                    bool develop,
                    bool evaluate) override {

        std::vector<TensorXf> inner_images;
        for (auto& integrator : m_integrators) {
            auto image = integrator->render(scene, sensor, seed, spp, develop, evaluate);
            inner_images.push_back(image);
        }

        TensorXf aovs_image;
        {
            aovs_image = Base::render(scene, sensor, seed, spp, develop, evaluate);

            // AOVs image above includes film target inner integrator channels as well so get slice
            // just with AOVs
            size_t num_aovs = m_aov_names.size() - m_integrator_aovs_count;
            if (develop)
                aovs_image = get_channels_slice(aovs_image, aovs_image.shape(2) - num_aovs, num_aovs);
        }

        if (develop)
            return merge_channels(inner_images, aovs_image);

        return {};
    }

    TensorXf render_forward(Scene* scene,
                            void* params,
                            Sensor *sensor,
                            uint32_t seed = 0,
                            uint32_t spp = 0) override {

        // Perform forward mode propagation just for AOV image
        TensorXf aovs_grad;
        {
            TensorXf aovs_image = Base::render(scene, sensor, seed, spp);

            // AOVs image above includes film target inner integrator channels as well so get slice
            // just with AOVs
            size_t num_aovs = m_aov_names.size() - m_integrator_aovs_count;
            aovs_image = get_channels_slice(aovs_image, aovs_image.shape(2) - num_aovs, num_aovs);

            // Perform an AD traversal of all registered AD variables that
            // influence 'aovs_image' in a differentiable manner
            if (dr::grad_enabled(aovs_image.array())) {
                dr::forward_to(aovs_image.array(), (uint32_t) dr::ADFlag::ClearInterior);
                aovs_grad = TensorXf(dr::grad(aovs_image.array()), 3, aovs_image.shape().data());
            } else {
                aovs_grad = TensorXf(dr::zeros<Float>(aovs_image.array().size()), 3, aovs_image.shape().data());
            }
        }

        // Let inner integrators handle forward differentiation for radiance
        std::vector<TensorXf> image_grads;
        for (auto& integrator : m_integrators)
            image_grads.push_back(integrator->render_forward(scene, params, sensor, seed, spp));

        return merge_channels(image_grads, aovs_grad);
    }

    void render_backward(Scene* scene,
                         void* params,
                         const TensorXf& grad_in,
                         Sensor* sensor,
                         uint32_t seed = 0,
                         uint32_t spp = 0) override {
        size_t base_ch_count = sensor->film()->base_channels_count();
        auto [image_grads, aovs_grad] = split_channels(base_ch_count, grad_in);

        // Perform AD back-propagation just for AOV image
        {
            TensorXf aovs_image = Base::render(scene, sensor, seed, spp);

            // AOVs image above includes film target inner integrator channels as well so get slice
            // just with AOVs
            size_t num_aovs = m_aov_names.size() - m_integrator_aovs_count;
            aovs_image = get_channels_slice(aovs_image, aovs_image.shape(2) - num_aovs, num_aovs);

            dr::backward_from((aovs_image * aovs_grad).array(), (uint32_t) dr::ADFlag::ClearInterior);
        }

        // Let inner integrators handle backwards differentiation for radiance
        for (size_t i = 0, N = image_grads.size(); i < N; ++i)
            m_integrators[i]->render_backward(scene, params, image_grads[i], sensor, seed, spp);
    }

    std::vector<std::string> aov_names() const override {
        return m_aov_names;
    }

    void traverse(TraversalCallback *callback) override {
        for (size_t i = 0; i < m_integrators.size(); ++i)
            callback->put_object("integrator_" + std::to_string(i),
                                 m_integrators[i],
                                 +ParamFlags::Differentiable);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AOVIntegrator[" << std::endl
            << "  aovs = " << m_aov_names << "," << std::endl
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

    MI_DECLARE_CLASS()
protected:

    TensorXf get_channels_slice(const TensorXf& src, size_t channel_offset, size_t num_channels) const {
        using Array = typename TensorXf::Array;

        size_t slice_shape[] = { src.shape(0), src.shape(1), num_channels };
        uint32_t slice_flat = (uint32_t) (slice_shape[0] * slice_shape[1] * slice_shape[2]);

        DynamicBuffer<UInt32> idx = dr::arange<DynamicBuffer<UInt32>>(slice_flat);
        DynamicBuffer<UInt32> pixel_idx = idx / num_channels;
        DynamicBuffer<UInt32> channel_idx = dr::fmadd(pixel_idx, uint32_t(-(int)num_channels), idx)
            + channel_offset;

        DynamicBuffer<UInt32> values_idx = dr::fmadd(pixel_idx, src.shape(2), channel_idx);
        return TensorXf(dr::gather<Array>(src.array(), values_idx), 3, slice_shape);
    }

    void set_channels_slice(const TensorXf& src, TensorXf& dst, size_t dst_channel_offset) const {
        auto* src_shape = src.shape().data();
        uint32_t src_flat = (uint32_t) (src_shape[0] * src_shape[1] * src_shape[2]);

        DynamicBuffer<UInt32> idx = dr::arange<DynamicBuffer<UInt32>>(src_flat);
        DynamicBuffer<UInt32> pixel_idx = idx / src_shape[2];
        DynamicBuffer<UInt32> dst_channel_idx = dr::fmadd(pixel_idx, uint32_t(-(int)src_shape[2]), idx)
            + dst_channel_offset;

        uint32_t num_dst_channels = (uint32_t) dst.shape(2);
        DynamicBuffer<UInt32> dst_values_idx = dr::fmadd(pixel_idx, num_dst_channels, dst_channel_idx);

        dr::scatter(
            dst.array(),
            src.array(),
            dst_values_idx);
    }

    /// Combine inner integrator images and AOVS image
    TensorXf merge_channels(const std::vector<TensorXf>& inner_images,
                            const TensorXf& aovs_image) const {
        using Array = typename TensorXf::Array;

        size_t num_aovs = m_aov_names.size();

        auto* aovs_image_shape = aovs_image.shape().data();

        // Figure out entire number of channels of combined image
        size_t combined_shape[3] = { aovs_image_shape[0], aovs_image_shape[1], num_aovs - m_integrator_aovs_count };
        for (const auto& image : inner_images)
            combined_shape[2] += image.shape(2);

        size_t combined_flat = combined_shape[0] * combined_shape[1] * combined_shape[2];
        TensorXf combined_image = TensorXf(dr::zeros<Array>(combined_flat), 3, combined_shape);

        // Load base channels from inner integrators into combined tensor
        uint32_t channel_offset = 0;
        for (const auto& image : inner_images) {
            set_channels_slice(image, combined_image, channel_offset);
            channel_offset += (uint32_t)image.shape(2);
        }

        // Load aovs image into combined
        set_channels_slice(aovs_image, combined_image, channel_offset);

        return combined_image;
    }

    /// Split up an image into image generated by inner-integrators and AOV image
    std::pair<std::vector<TensorXf>, TensorXf> split_channels(size_t base_channel_count, const TensorXf& combined_image) const {
        using Array = typename TensorXf::Array;

        std::vector<TensorXf> inner_images;

        size_t channel_offset = 0;
        for (const auto& integrator : m_integrators) {
            size_t image_channels = base_channel_count + integrator->aov_names().size();
            auto image = get_channels_slice(combined_image, channel_offset, image_channels);
            inner_images.push_back(image);

            channel_offset += image_channels;
        }

        size_t num_aovs = m_aov_names.size() - m_integrator_aovs_count;
        TensorXf aovs_image = get_channels_slice(combined_image, channel_offset, num_aovs);

        return { inner_images, aovs_image };
    }

private:
    size_t m_integrator_aovs_count;
    std::vector<Type> m_aov_types;
    std::vector<std::string> m_aov_names;
    std::vector<ref<Base>> m_integrators;
};

MI_IMPLEMENT_CLASS_VARIANT(AOVIntegrator, SamplingIntegrator)
MI_EXPORT_PLUGIN(AOVIntegrator, "AOV integrator");
NAMESPACE_END(mitsuba)
