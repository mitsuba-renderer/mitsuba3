#include <mitsuba/render/film.h>
#include <drjit/idiv.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

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

    // Use the provided reconstruction filter, if any.
    for (auto &prop : props.objects()) {
        if (ReconstructionFilter *rfilter = prop.try_get<ReconstructionFilter>()) {
            if (m_filter)
                Throw("A film can only have one reconstruction filter.");

            m_filter = rfilter;
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
        << "  m_filter = "      << m_filter        << std::endl
        << "]";
    return oss.str();
}

MI_IMPLEMENT_TRAVERSE_CB(Film, Object)
MI_INSTANTIATE_CLASS(Film)
NAMESPACE_END(mitsuba)
