#include <mitsuba/render/film.h>
#include <mitsuba/render/postprocess.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <drjit/idiv.h>
#include <drjit/tensor.h>
#include <algorithm>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Film<Float, Spectrum>::Film(const Properties &props)
    : JitObject<Film>(props.id()) {
    bool is_m_film = string::to_lower(props.plugin_name()) == "mfilm";

    // Horizontal and vertical film resolution in pixels
    m_size = ScalarVector2u(
        props.get<uint32_t>("width", is_m_film ? 1 : 768),
        props.get<uint32_t>("height", is_m_film ? 1 : 576)
    );

    // Crop window specified in pixels - by default, this matches the full sensor area.
    ScalarVector2u crop_size = ScalarVector2u(
        props.get<uint32_t>("crop_width", m_size.x()),
        props.get<uint32_t>("crop_height", m_size.y())
    );

    ScalarPoint2u crop_offset = ScalarPoint2u(
        props.get<uint32_t>("crop_offset_x", 0),
        props.get<uint32_t>("crop_offset_y", 0)
    );

    set_crop_window(crop_offset, crop_size);

    // If set to true, regions slightly outside of the film plane will also be
    // sampled, which improves the image quality at the edges especially with
    // large reconstruction filters.
    m_sample_border = props.get<bool>("sample_border", false);

    // Use the provided reconstruction filter and post-processing stages, if any.
    for (auto &prop : props.objects()) {
        if (ReconstructionFilter *rfilter = prop.try_get<ReconstructionFilter>()) {
            if (m_filter)
                Throw("A film can only have one reconstruction filter.");

            m_filter = rfilter;
        } else if (PostProcess *postprocess = prop.try_get<PostProcess>()) {
            m_postprocess.push_back(postprocess);
        }
    }

    // Use a Gaussian reconstruction filter if none has been specified
    if (!m_filter)
        m_filter =
            PluginManager::instance()->create_object<ReconstructionFilter>(
                Properties("gaussian"));

    update_launch_params();
}

MI_VARIANT Film<Float, Spectrum>::~Film() { }

MI_VARIANT void Film<Float, Spectrum>::traverse(TraversalCallback *cb) {
    cb->put("size",        m_size,        ParamFlags::NonDifferentiable);
    cb->put("crop_size",   m_crop_size,   ParamFlags::NonDifferentiable);
    cb->put("crop_offset", m_crop_offset, ParamFlags::NonDifferentiable);
    for (auto &stage : m_postprocess) {
        std::string_view id = stage->id();
        if (id.empty() || string::starts_with(id, "_unnamed_"))
            id = "";
        cb->put(id, stage, ParamFlags::Differentiable);
    }
}

MI_VARIANT void Film<Float, Spectrum>::parameters_changed(const std::vector<std::string> &keys) {
    ScalarVector2u crop_size = m_crop_size;
    ScalarPoint2u crop_offset = m_crop_offset;

    // Reset the crop window to match the full sensor area if necessary
    if (string::contains(keys, "size")) {
        if (!string::contains(keys, "crop_size"))
            crop_size = ScalarPoint2u(m_size);
        if (!string::contains(keys, "crop_offset"))
            crop_offset = 0;
    }

    set_crop_window(crop_offset, crop_size);
}

MI_VARIANT void
Film<Float, Spectrum>::prepare_sample(const UnpolarizedSpectrum & /* spec */,
                                      const Wavelength & /* wavelengths */,
                                      Float * /* aovs */,
                                      Float /* weight */,
                                      Float /* alpha */,
                                      Mask /* active */) const {
    NotImplementedError("prepare_sample");
}

MI_VARIANT const typename Film<Float, Spectrum>::Texture *
Film<Float, Spectrum>::sensor_response_function() {
    return m_srf.get();
}

MI_VARIANT void Film<Float, Spectrum>::set_crop_window(const ScalarPoint2u &crop_offset,
                                                       const ScalarVector2u &crop_size) {
    if (dr::any(crop_offset + crop_size > m_size))
        Throw("Invalid crop window specification: crop_offset(%u, %u) + "
              "crop_size(%u, %u) > size(%u, %u)", crop_offset.x(), crop_offset.y(),
              crop_size.x(), crop_size.y(), m_size.x(), m_size.y());

    m_crop_size   = crop_size;
    m_crop_offset = crop_offset;
    update_launch_params();
}

MI_VARIANT void Film<Float, Spectrum>::update_launch_params() {
    if constexpr (dr::is_jit_v<Float>) {
        // The constructor calls set_crop_window() before the filter exists
        if (!m_filter)
            return;

        uint32_t width = m_crop_size.x();
        if (m_sample_border)
            width += 2 * m_filter->border_size();

        dr::divisor<uint32_t> div(dr::maximum(width, 1u));

        // Packet gathers need a power of two size, hence the padding
        uint32_t data[8] = { m_crop_size.x(), m_crop_size.y(), m_crop_offset.x(),
                             m_crop_offset.y(), div.multiplier, div.shift, 0, 0 };

        m_launch_params = dr::load<UInt32>(data, 8);
    }
}

MI_VARIANT typename Film<Float, Spectrum>::LaunchParams
Film<Float, Spectrum>::launch_params() const {
    LaunchParams lp;
    if constexpr (dr::is_jit_v<Float>) {
        auto p = dr::gather<dr::Array<UInt32, 8>>(m_launch_params, UInt32(0));
        lp.crop_size   = Vector2u(p[0], p[1]);
        lp.crop_offset = Point2u(p[2], p[3]);
        lp.width_mul   = p[4];
        lp.width_shift = p[5];
    } else {
        lp.crop_size   = m_crop_size;
        lp.crop_offset = m_crop_offset;
        uint32_t width = m_crop_size.x();
        if (m_sample_border)
            width += 2 * m_filter->border_size();
        dr::divisor<uint32_t> div(width);
        lp.width_mul   = div.multiplier;
        lp.width_shift = div.shift;
    }
    return lp;
}

MI_VARIANT void Film<Float, Spectrum>::set_size(const ScalarPoint2u &size) {
    m_size = size;
    // Reset the crop window to match the full sensor area
    set_crop_window(ScalarVector2u(0, 0), m_size);
}

MI_VARIANT std::string Film<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "Film[" << std::endl
        << "  size = "          << m_size          << "," << std::endl
        << "  crop_size = "     << m_crop_size     << "," << std::endl
        << "  crop_offset = "   << m_crop_offset   << "," << std::endl
        << "  sample_border = " << m_sample_border << "," << std::endl
        << "  m_filter = "      << m_filter        << "," << std::endl
        << "  postprocess = "   << m_postprocess   << std::endl
        << "]";
    return oss.str();
}

MI_VARIANT typename Film<Float, Spectrum>::TensorXf
Film<Float, Spectrum>::apply_postprocess(
    const TensorXf &image, const std::vector<std::string> &channels) const {
    TensorXf result = image;

    for (size_t i = 0; i < m_postprocess.size(); ++i) {
        TensorXf output = m_postprocess[i]->eval(result, channels);

        bool same_shape = output.ndim() == 3;
        for (size_t j = 0; same_shape && j < 3; ++j)
            same_shape = output.shape(j) == result.shape(j);
        if (!same_shape)
            Throw("apply_postprocess(): post-processing stage %zu changed the "
                  "shape of the image. Stages must preserve the shape of "
                  "their input.", i);

        if constexpr (dr::is_diff_v<Float>) {
            if (dr::grad_enabled(result.array()) &&
                !dr::grad_enabled(output.array()))
                Throw("apply_postprocess(): post-processing stage %zu severed "
                      "the AD graph of the image. In differentiable rendering, "
                      "stages must compute their output from the input using "
                      "Dr.Jit operations.", i);
        }

        result = std::move(output);
    }

    return result;
}

MI_VARIANT ref<Bitmap> Film<Float, Spectrum>::apply_postprocess(Bitmap *image) const {
    if (m_postprocess.empty())
        return image;

    if (image->component_format() != struct_type_v<ScalarFloat>)
        Throw("apply_postprocess(): expected a bitmap with %s components",
              struct_type_v<ScalarFloat>);

    std::vector<std::string> channels = image->channel_names();

    TensorXf tensor(image->data(), { (size_t) image->height(),
                                     (size_t) image->width(),
                                     image->channel_count() });

    tensor = apply_postprocess(tensor, channels);

    auto &&storage = dr::migrate(tensor.array(), JitBackend::None);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    ref<Bitmap> result =
        new Bitmap(image->pixel_format(), struct_type_v<ScalarFloat>,
                   image->size(), image->channel_count(), channels);
    result->set_srgb_gamma(image->srgb_gamma());
    result->set_premultiplied_alpha(image->premultiplied_alpha());
    result->set_metadata(image->metadata());
    memcpy(result->data(), storage.data(), result->buffer_size());

    return result;
}

MI_VARIANT bool Film<Float, Spectrum>::is_ldr_format(const fs::path &path) {
    std::string ext = string::to_lower(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".tga" || ext == ".ppm";
}

MI_VARIANT void Film<Float, Spectrum>::write_ldr(const Bitmap *image,
                                                 const fs::path &path) const {
    if (image->component_format() != struct_type_v<ScalarFloat>)
        Throw("write_ldr(): expected a bitmap with %s components",
              struct_type_v<ScalarFloat>);

    #if !defined(_WIN32)
        Log(Info, "\U00002714  Developing \"%s\" ..", path.string());
    #else
        Log(Info, "Developing \"%s\" ..", path.string());
    #endif

    // 8-bit formats store color and alpha; other channels are dropped
    std::vector<std::string> channels = image->channel_names();
    bool mono  = channels[0] == "Y",
         alpha = std::find(channels.begin(), channels.end(), "A") != channels.end();
    std::string ext = string::to_lower(path.extension().string());
    if (ext == ".jpg" || ext == ".jpeg")
        alpha = false;

    Bitmap::PixelFormat pixel_format =
        mono ? (alpha ? Bitmap::PixelFormat::YA : Bitmap::PixelFormat::Y)
             : (alpha ? Bitmap::PixelFormat::RGBA : Bitmap::PixelFormat::RGB);

    ref<Bitmap> target = new Bitmap(pixel_format, struct_type_v<uint8_t>,
                                    image->size());
    target->set_srgb_gamma(true);
    image->convert(target);
    target->write(path);
}

MI_IMPLEMENT_TRAVERSE_CB(Film, Object)
MI_INSTANTIATE_CLASS(Film)
NAMESPACE_END(mitsuba)
