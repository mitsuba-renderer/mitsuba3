#include <mitsuba/render/film.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/postprocess.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <drjit/idiv.h>
#include <drjit/tensor.h>
#include <cstring>
#include <set>

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

    // Use the provided reconstruction filter and post-processing filters, if any.
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

    std::string file_format = string::to_lower(
        props.get<std::string_view>("file_format", "openexr"));
    if (file_format == "openexr" || file_format == "exr")
        m_file_format = Bitmap::FileFormat::OpenEXR;
    else if (file_format == "rgbe")
        m_file_format = Bitmap::FileFormat::RGBE;
    else if (file_format == "pfm")
        m_file_format = Bitmap::FileFormat::PFM;
    else
        Throw("The \"file_format\" parameter must either be equal to "
              "\"openexr\", \"pfm\", or \"rgbe\", found %s instead.",
              file_format);

    std::string component_format = string::to_lower(
        props.get<std::string_view>("component_format", "float16"));
    if (component_format == "float16")
        m_component_format = sj::Type::Float16;
    else if (component_format == "float32")
        m_component_format = sj::Type::Float32;
    else if (component_format == "uint32")
        m_component_format = sj::Type::UInt32;
    else
        Throw("The \"component_format\" parameter must either be equal to "
              "\"float16\", \"float32\", or \"uint32\". Found %s instead.",
              component_format);

    if (props.has_property("compensate")) {
        props.mark_queried("compensate");
        Log(Warn, "The \"compensate\" (Kahan-style error-compensated "
                  "accumulation) parameter has been removed and is now "
                  "ignored.");
    }

    update_launch_params();
}

MI_VARIANT Film<Float, Spectrum>::~Film() { }

MI_VARIANT void Film<Float, Spectrum>::traverse(TraversalCallback *cb) {
    cb->put("size",        m_size,        ParamFlags::NonDifferentiable);
    cb->put("crop_size",   m_crop_size,   ParamFlags::NonDifferentiable);
    cb->put("crop_offset", m_crop_offset, ParamFlags::NonDifferentiable);
    for (auto &postprocess : m_postprocess) {
        std::string_view id = postprocess->id();
        if (id.empty() || string::starts_with(id, "_unnamed_"))
            id = "";
        cb->put(id, postprocess, ParamFlags::Differentiable);
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
    alloc_storage();
}

// -----------------------------------------------------------------------------
//                            Channels and samples
// -----------------------------------------------------------------------------

MI_VARIANT void
Film<Float, Spectrum>::prepare_sample(const UnpolarizedSpectrum &spec,
                                      const Wavelength &wavelengths,
                                      Float *out, Mask valid,
                                      Mask active) const {
    DRJIT_MARK_USED(wavelengths);
    DRJIT_MARK_USED(active);

    Color3f rgb;
    if constexpr (is_spectral_v<Spectrum>)
        rgb = spectrum_to_srgb(spec, wavelengths, active);
    else if constexpr (is_monochromatic_v<Spectrum>)
        rgb = spec.x();
    else
        rgb = spec;

    out[0] = rgb.x();
    out[1] = rgb.y();
    out[2] = rgb.z();

    if (m_base_channels.back() == "A")
        out[3] = dr::select(valid, Float(1.f), Float(0.f));
}

MI_VARIANT size_t Film<Float, Spectrum>::prepare(const std::vector<std::string> &aovs) {
    m_channels = m_base_channels;
    m_channels.insert(m_channels.end(), aovs.begin(), aovs.end());

    std::set<std::string> unique(m_channels.begin(), m_channels.end());
    unique.insert("W");
    if (unique.size() != m_channels.size() + 1)
        Throw("Film::prepare(): duplicate channel name in %s", m_channels);

    alloc_storage();

    return m_channels.size() + 1;
}

MI_VARIANT void Film<Float, Spectrum>::alloc_storage() {
    // Films that don't name their base channels store RGB
    if (m_base_channels.empty())
        m_base_channels = { "R", "G", "B" };

    // The storage holds the base channels until prepare() adds AOVs
    if (m_channels.empty())
        m_channels = m_base_channels;

    size_t channel_count = m_channels.size() + 1;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_storage != nullptr &&
        m_storage->channel_count() == channel_count &&
        dr::all(m_storage->size() == m_crop_size) &&
        dr::all(m_storage->offset() == m_crop_offset))
        m_storage->clear();
    else
        m_storage = new ImageBlock(m_crop_size, m_crop_offset,
                                   (uint32_t) channel_count);
}

MI_VARIANT ref<typename Film<Float, Spectrum>::ImageBlock>
Film<Float, Spectrum>::create_block(const ScalarVector2u &size, bool normalize,
                                    bool border) {
    // Report suspicious samples in scalar mode, unless AOVs may legitimately be negative
    bool warn = !dr::is_jit_v<Float> && !is_spectral_v<Spectrum> &&
                m_channels.size() == m_base_channels.size();

    bool default_config = dr::all(size == ScalarVector2u(0));

    ref<ImageBlock> block = new ImageBlock(
        default_config ? m_crop_size : size,
        default_config ? m_crop_offset : ScalarPoint2u(0),
        m_storage->channel_count(), m_filter.get(), border, normalize,
        dr::is_jit_v<Float> /* coalesce */, warn /* warn_negative */,
        warn /* warn_invalid */);

    if (default_config) {
        LaunchParams lp = launch_params();
        block->set_opaque_geometry(lp.crop_size, Point2i(lp.crop_offset));
    }

    return block;
}

MI_VARIANT void Film<Float, Spectrum>::put_block(const ImageBlock *block) {
    Assert(m_storage != nullptr);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_storage->put_block(block);
}

MI_VARIANT void Film<Float, Spectrum>::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_storage)
        m_storage->clear();
}

// -----------------------------------------------------------------------------
//                              Developing images
// -----------------------------------------------------------------------------

MI_VARIANT typename Film<Float, Spectrum>::TensorXf
Film<Float, Spectrum>::storage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_storage->tensor();
}

MI_VARIANT typename Film<Float, Spectrum>::TensorXf
Film<Float, Spectrum>::develop(bool postprocess) const {
    using Array = typename TensorXf::Array;

    TensorXf raw = storage();
    const Array &data = raw.array();
    size_t height = raw.shape(0), width = raw.shape(1);
    uint32_t target_ch = (uint32_t) m_channels.size(),
             source_ch = target_ch + 1,
             pixel_count = (uint32_t) (width * height);

    // Divide the stored channels by the trailing weight channel
    Array values;
    if constexpr (dr::is_jit_v<Float>) {
        UInt32 idx         = dr::arange<UInt32>(pixel_count * target_ch),
               pixel_idx   = idx / target_ch,
               channel_idx = dr::fmadd(pixel_idx, uint32_t(-(int) target_ch), idx);

        Float weight = dr::gather<Float>(data, dr::fmadd(pixel_idx, source_ch, target_ch));
        values = dr::gather<Float>(data, dr::fmadd(pixel_idx, source_ch, channel_idx));
        values /= dr::select(weight == 0.f, 1.f, weight);
    } else {
        values = dr::empty<Array>((size_t) pixel_count * target_ch);
        const ScalarFloat *src = data.data();
        ScalarFloat *dst = values.data();

        for (uint32_t p = 0; p < pixel_count; ++p) {
            ScalarFloat weight = src[p * source_ch + target_ch],
                        scale  = weight == 0.f ? 1.f : 1.f / weight;
            for (uint32_t c = 0; c < target_ch; ++c)
                dst[p * target_ch + c] = src[p * source_ch + c] * scale;
        }
    }

    TensorXf image(values, { height, width, (size_t) target_ch });

    if (!postprocess || m_postprocess.empty())
        return image;

    return apply_postprocess(image, m_channels);
}

MI_VARIANT ref<Bitmap> Film<Float, Spectrum>::bitmap(bool postprocess) const {
    TensorXf image = develop(postprocess);

    auto &&host = dr::migrate(image.array(), JitBackend::None);
    if constexpr (dr::is_jit_v<Float>)
        dr::sync_thread();

    ref<Bitmap> result = new Bitmap(
        Bitmap::pixel_format_from_channels(m_channels), struct_type_v<ScalarFloat>,
        ScalarVector2u((uint32_t) image.shape(1), (uint32_t) image.shape(0)),
        m_channels.size(), m_channels);

    memcpy(result->data(), host.data(), result->buffer_size());

    return result;
}

MI_VARIANT void Film<Float, Spectrum>::write(const fs::path &path) const {
    fs::path filename = path;
    std::string extension = string::to_lower(filename.extension().string());

    // 8-bit formats store the sRGB-encoded base channels, which requires
    // a film with color base channels
    bool ldr = extension == ".png" || extension == ".jpg" ||
               extension == ".jpeg" || extension == ".bmp" ||
               extension == ".tga" || extension == ".ppm";

    Bitmap::PixelFormat base_format =
        Bitmap::pixel_format_from_channels(base_channels());
    if (ldr && base_format == Bitmap::PixelFormat::MultiChannel)
        Throw("write(): the file format \"%s\" only supports images with "
              "color channels, but this film produces channels %s.",
              extension, base_channels());

    if (!ldr) {
        const char *proper_extension = ".pfm";
        if (m_file_format == Bitmap::FileFormat::OpenEXR)
            proper_extension = ".exr";
        else if (m_file_format == Bitmap::FileFormat::RGBE)
            proper_extension = ".rgbe";

        if (extension != proper_extension)
            filename.replace_extension(proper_extension);
    }

    #if !defined(_WIN32)
        Log(Info, "\U00002714  Developing \"%s\" ..", filename.string());
    #else
        Log(Info, "Developing \"%s\" ..", filename.string());
    #endif

    ref<Bitmap> source = bitmap();
    if (ldr) {
        bool mono  = base_format == Bitmap::PixelFormat::Y ||
                     base_format == Bitmap::PixelFormat::YA,
             alpha = (base_format == Bitmap::PixelFormat::YA ||
                      base_format == Bitmap::PixelFormat::RGBA ||
                      base_format == Bitmap::PixelFormat::XYZA) &&
                     extension != ".jpg" && extension != ".jpeg";

        Bitmap::PixelFormat pixel_format =
            mono ? (alpha ? Bitmap::PixelFormat::YA : Bitmap::PixelFormat::Y)
                 : (alpha ? Bitmap::PixelFormat::RGBA : Bitmap::PixelFormat::RGB);

        source->convert(pixel_format, struct_type_v<uint8_t>, true)
              ->write(filename);
    } else if (m_component_format != struct_type_v<ScalarFloat>) {
        ref<Bitmap> target = new Bitmap(
            source->pixel_format(), m_component_format, source->size(),
            source->channel_count(), source->channel_names());
        source->convert(target);
        target->write(filename, m_file_format);
    } else {
        source->write(filename, m_file_format);
    }
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
            Throw("apply_postprocess(): post-processing filter %zu changed the "
                  "shape of the image. Filters must preserve the shape of "
                  "their input.", i);

        result = std::move(output);
    }

    return result;
}

// -----------------------------------------------------------------------------
//                            Geometry and misc.
// -----------------------------------------------------------------------------

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

MI_IMPLEMENT_TRAVERSE_CB(Film, Object)
MI_INSTANTIATE_CLASS(Film)
NAMESPACE_END(mitsuba)
