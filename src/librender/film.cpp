#include <mitsuba/render/film.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Film<Float, Spectrum>::Film(const Properties &props) : Object() {
    bool is_m_film = string::to_lower(props.plugin_name()) == "mfilm";

    // Horizontal and vertical film resolution in pixels
    m_size = ScalarVector2i(
        props.int_("width", is_m_film ? 1 : 768),
        props.int_("height", is_m_film ? 1 : 576)
    );

    // Crop window specified in pixels - by default, this matches the full sensor area.
    ScalarPoint2i crop_offset = ScalarPoint2i(
        props.int_("crop_offset_x", 0),
        props.int_("crop_offset_y", 0)
    );

    ScalarVector2i crop_size = ScalarVector2i(
        props.int_("crop_width", m_size.x()),
        props.int_("crop_height", m_size.y())
    );

    set_crop_window(crop_offset, crop_size);

    /* If set to true, regions slightly outside of the film plane will also be
       sampled, which improves the image quality at the edges especially with
       large reconstruction filters. */
    m_high_quality_edges = props.bool_("high_quality_edges", false);

    // Use the provided reconstruction filter, if any.
    for (auto &[name, obj] : props.objects(false)) {
        auto *rfilter = dynamic_cast<ReconstructionFilter *>(obj.get());
        if (rfilter) {
            if (m_filter)
                Throw("A film can only have one reconstruction filter.");
            m_filter = rfilter;
            props.mark_queried(name);
        }
    }

    if (!m_filter) {
        // No reconstruction filter has been selected. Load a Gaussian filter by default
        m_filter =
            PluginManager::instance()->create_object<ReconstructionFilter>(Properties("gaussian"));
    }
}

MTS_VARIANT Film<Float, Spectrum>::~Film() {}

MTS_VARIANT void Film<Float, Spectrum>::set_crop_window(const ScalarPoint2i &crop_offset,
                                                        const ScalarVector2i &crop_size) {
    if (any(crop_offset < 0 || crop_size <= 0 || crop_offset + crop_size > m_size))
        Throw("Invalid crop window specification!\n"
              "offset %s + crop size %s vs full size %s",
              crop_offset, crop_size, m_size);

    m_crop_size   = crop_size;
    m_crop_offset = crop_offset;
}

MTS_VARIANT std::string Film<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "Film[" << std::endl
        << "  size = "        << m_size        << "," << std::endl
        << "  crop_size = "   << m_crop_size   << "," << std::endl
        << "  crop_offset = " << m_crop_offset << "," << std::endl
        << "  high_quality_edges = " << m_high_quality_edges << "," << std::endl
        << "  m_filter = " << m_filter << std::endl
        << "]";
    return oss.str();
}


MTS_IMPLEMENT_CLASS_VARIANT(Film, Object, "film")
MTS_INSTANTIATE_CLASS(Film)
NAMESPACE_END(mitsuba)
