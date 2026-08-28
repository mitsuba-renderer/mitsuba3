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
(`BSDF::eval_diffuse_reflectance()`) of the material. Note that depending on
the material, this value might only be an approximation.
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
        dUVdx,
        dUVdy,
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
            } else if (item[1] == "duv_dx") {
                m_aov_types.push_back(AOVType::dUVdx);
                m_aov_names.push_back(item[0] + ".U");
                m_aov_names.push_back(item[0] + ".V");
            } else if (item[1] == "duv_dy") {
                m_aov_types.push_back(AOVType::dUVdy);
                m_aov_names.push_back(item[0] + ".U");
                m_aov_names.push_back(item[0] + ".V");
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
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler * /*sampler*/,
                                     const RayDifferential3f &ray,
                                     const Medium * /*medium*/,
                                     Float *aovs,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        std::pair<Spectrum, Mask> result { 0.f, false };
        SurfaceInteraction3f si =
            scene->ray_intersect(ray, (uint32_t) RayFlags::Default, true, false, 0, 0, active);
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

        for (size_t i = 0; i < m_aov_types.size(); ++i) {
            switch (m_aov_types[i]) {
                case AOVType::Albedo: {
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
                        Frame3f sh_frame = dr::zeros<Frame3f>();
                        if (dr::any_or<true>(si.is_valid()))
                        {
                            Mask valid = active && si.is_valid();
                            BSDFPtr m_bsdf = si.bsdf(ray);
                            sh_frame = m_bsdf->sh_frame(si, valid);
                        }
                        *aovs++ = sh_frame.n.x();
                        *aovs++ = sh_frame.n.y();
                        *aovs++ = sh_frame.n.z();
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

                case AOVType::dUVdx:
                    si.compute_uv_partials(ray);
                    *aovs++ = si.duv_dx.x();
                    *aovs++ = si.duv_dx.y();
                    break;

                case AOVType::dUVdy:
                    si.compute_uv_partials(ray);
                    *aovs++ = si.duv_dy.x();
                    *aovs++ = si.duv_dy.y();
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
                    bool evaluate) override {

        // Prepare shape indexing data structure for scalar variants
        if constexpr (!dr::is_jit_v<Float>) {
            if (m_has_shape_index_aov) {
                m_shape_to_idx.clear();
                size_t counter = 1;
                for (const ref<Shape>& shape : scene->shapes())
                    m_shape_to_idx[shape.get()] = (uint32_t) counter++;
            }
        }
        return Base::render(scene, sensor, seed, spp, develop, evaluate);
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
    std::unordered_map<const Shape*, uint32_t> m_shape_to_idx;
};

template <typename Float, typename Spectrum>
class AOVIntegrator final : public SamplingIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(SamplingIntegrator)
    MI_IMPORT_TYPES(Scene, Shape, Sensor, Sampler, Medium, BSDFPtr, ShapePtr, Film, ImageBlock)
    using Impl = AOVIntegratorImpl<Float, Spectrum>;

    AOVIntegrator(const Properties &props) : Base(props),
        m_nested_aovs_count(0) {

        // Collect integrators and their AOV / RGBA channels
        for (auto &prop : props.objects()) {
            Base *integrator = prop.try_get<Base>();
            if (!integrator)
                Throw("Child objects must be of type 'SamplingIntegrator'!");
            std::string name(prop.name());
            m_integrators.push_back(integrator);
            m_aov_names.push_back(name + ".R");
            m_aov_names.push_back(name + ".G");
            m_aov_names.push_back(name + ".B");
            m_aov_names.push_back(name + ".A");

            std::vector<std::string> child_aovs = integrator->aov_names();
            for (const auto &child_aov : child_aovs)
                m_aov_names.push_back(name + "." + child_aov);
        }

        std::string_view aovs = props.get<std::string_view>("aovs");
        if (!aovs.empty()) {
            ref<Impl> aov_integrator = new Impl(aovs);
            if (!aov_integrator->aov_names().empty()) {
                m_aov_integrator = aov_integrator;
                for (const auto &name : m_aov_integrator->aov_names())
                    m_aov_names.push_back(name);
                m_nested_aovs_count = m_aov_integrator->aov_names().size();
            }
        }

        if (m_aov_names.empty())
            Log(Warn, "No AOVs were specified!");
    }


    TensorXf render(Scene *scene,
                    Sensor *sensor,
                    UInt32 seed,
                    uint32_t spp,
                    bool develop,
                    bool evaluate) override {
        Film *film = sensor->film();
        size_t base_channel_count = film->base_channels_count();
        size_t raw_channel_count = base_channel_count + 1 /* W channel */;

        std::vector<TensorXf> inner_images, inner_raw_tensors;
        for (auto& integrator : m_integrators) {
            auto image = integrator->render(scene, sensor, seed, spp, develop, evaluate);
            inner_images.push_back(image);
            inner_raw_tensors.push_back(film->develop(true));
        }

        TensorXf aovs_image, aovs_raw_tensor;
        if (m_aov_integrator) {
            aovs_image = m_aov_integrator->render(scene, sensor, seed, spp, develop, evaluate);
            aovs_raw_tensor = film->develop(true);
            if (develop)
                aovs_image = get_channels_slice(aovs_image, base_channel_count, m_nested_aovs_count);
        }

        // Propagate nested integrator results to the shared film
        ScalarVector2u crop_size = film->crop_size();
        size_t total_raw_channels = raw_channel_count + m_aov_names.size();
        size_t assembled_shape[3] = { crop_size.y(), crop_size.x(), total_raw_channels };
        size_t n_elements = crop_size.y() * crop_size.x() * total_raw_channels;
        TensorXf assembled_raw(dr::zeros<typename TensorXf::Array>(n_elements), 3, assembled_shape);
        if (!inner_raw_tensors.empty()) {
            copy_channels_slice(inner_raw_tensors[0], 0, assembled_raw, 0, raw_channel_count);
        } else if (m_aov_integrator) {
            copy_channels_slice(aovs_raw_tensor, 0, assembled_raw, 0, raw_channel_count);
        }
        uint32_t dst_offset = (uint32_t) raw_channel_count;
        for (size_t i = 0; i < m_integrators.size(); ++i) {
            copy_channels_slice(inner_raw_tensors[i], 0, assembled_raw, dst_offset, 4);
            dst_offset += 4;
            uint32_t child_aov_count = (uint32_t) m_integrators[i]->aov_names().size();
            if (child_aov_count > 0) {
                copy_channels_slice(inner_raw_tensors[i], raw_channel_count, assembled_raw, dst_offset, child_aov_count);
                dst_offset += child_aov_count;
            }
        }
        if (m_aov_integrator) {
            copy_channels_slice(aovs_raw_tensor, raw_channel_count, assembled_raw, dst_offset, m_nested_aovs_count);
            dst_offset += (uint32_t) m_nested_aovs_count;
        }
        film->prepare(m_aov_names);
        film->clear();
        ref<ImageBlock> block = new ImageBlock(assembled_raw, film->crop_offset());
        film->put_block(block);

        if (develop) {
            TensorXf result = merge_channels(inner_images, aovs_image);
            if (evaluate)
                dr::eval(result);
            return result;
        }

        return {};
    }

    TensorXf render_forward(Scene* scene,
                            void* params,
                            Sensor *sensor,
                            UInt32 seed = 0,
                            uint32_t spp = 0) override {

        // Perform forward mode propagation just for AOV image
        TensorXf aovs_grad;
        if (m_aov_integrator) {
            TensorXf aovs_image = m_aov_integrator->render(scene, sensor, seed, spp);
            // Extract only the AOV channel, omitting the RGB/RGBA default channels
            aovs_image = get_channels_slice(aovs_image, sensor->film()->base_channels_count(), m_nested_aovs_count);

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
                         UInt32 seed = 0,
                         uint32_t spp = 0) override {
        size_t base_ch_count = sensor->film()->base_channels_count();
        auto [image_grads, aovs_grad] = split_channels(base_ch_count, grad_in);

        // Perform AD back-propagation just for AOV image
        if (m_aov_integrator) {
            TensorXf aovs_image = m_aov_integrator->render(scene, sensor, seed, spp);
            // Extract only the AOV channel, omitting the RGB/RGBA default channels
            aovs_image = get_channels_slice(aovs_image, base_ch_count, m_nested_aovs_count);

            dr::backward_from((aovs_image * aovs_grad).array(), dr::ADFlag::ClearInterior | dr::ADFlag::AllowNoGrad);
        }

        // Let inner integrators handle backwards differentiation for radiance
        for (size_t i = 0, N = image_grads.size(); i < N; ++i)
            m_integrators[i]->render_backward(scene, params, image_grads[i], sensor, seed, spp);
    }

    std::vector<std::string> aov_names() const override {
        return m_aov_names;
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

    MI_DECLARE_CLASS(AOVIntegrator)
protected:

    void copy_channels_slice(const TensorXf& src, size_t src_channel_offset,
                             TensorXf& dst, size_t dst_channel_offset,
                             size_t num_channels) const {
        auto* src_shape = src.shape().data();
        uint32_t src_flat = (uint32_t) (src_shape[0] * src_shape[1] * num_channels);

        DynamicBuffer<UInt32> idx = dr::arange<DynamicBuffer<UInt32>>(src_flat);
        DynamicBuffer<UInt32> pixel_idx = idx / num_channels;
        DynamicBuffer<UInt32> channel_offset = dr::fmadd(pixel_idx, uint32_t(-(int)num_channels), idx);

        DynamicBuffer<UInt32> src_idx = dr::fmadd(pixel_idx, (uint32_t) src.shape(2), channel_offset + (uint32_t) src_channel_offset);
        DynamicBuffer<UInt32> dst_idx = dr::fmadd(pixel_idx, (uint32_t) dst.shape(2), channel_offset + (uint32_t) dst_channel_offset);

        dr::scatter(
            dst.array(),
            dr::gather<typename TensorXf::Array>(src.array(), src_idx),
            dst_idx);
    }

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

        auto* shape = !inner_images.empty() ? inner_images[0].shape().data() : aovs_image.shape().data();

        // Figure out entire number of channels of combined image
        size_t combined_shape[3] = { shape[0], shape[1], m_nested_aovs_count };
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
        if (m_aov_integrator)
            set_channels_slice(aovs_image, combined_image, channel_offset);

        return combined_image;
    }

    /// Split up an image into image generated by inner-integrators and AOV image
    std::pair<std::vector<TensorXf>, TensorXf> split_channels(size_t base_channel_count, const TensorXf& combined_image) const {
        std::vector<TensorXf> inner_images;

        size_t channel_offset = 0;
        for (const auto& integrator : m_integrators) {
            size_t image_channels = base_channel_count + integrator->aov_names().size();
            auto image = get_channels_slice(combined_image, channel_offset, image_channels);
            inner_images.push_back(image);

            channel_offset += image_channels;
        }

        TensorXf aovs_image;
        if (m_aov_integrator)
            aovs_image = get_channels_slice(combined_image, channel_offset, m_nested_aovs_count);

        return { inner_images, aovs_image };
    }

private:
    size_t m_nested_aovs_count;
    std::vector<std::string> m_aov_names;
    std::vector<ref<Base>> m_integrators;
    ref<Base> m_aov_integrator;
    MI_TRAVERSE_CB(Base, m_integrators)
};

MI_EXPORT_PLUGIN(AOVIntegrator)
NAMESPACE_END(mitsuba)
