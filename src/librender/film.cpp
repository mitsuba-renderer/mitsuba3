#include <mitsuba/render/film.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
Film<Float, Spectrum>::Film(const Properties &props) : Object() {
    bool is_m_film = string::to_lower(props.plugin_name()) == "mfilm";

    // Horizontal and vertical film resolution in pixels
    m_size = Vector2i(
        props.int_("width", is_m_film ? 1 : 768),
        props.int_("height", is_m_film ? 1 : 576)
    );
    // Crop window specified in pixels - by default, this matches the full sensor area.
    m_crop_offset = Point2i(
        props.int_("crop_offset_x", 0),
        props.int_("crop_offset_y", 0)
    );
    m_crop_size = Vector2i(
        props.int_("crop_width", m_size.x()),
        props.int_("crop_height", m_size.y())
    );
    check_valid_crop_window();

    /* If set to true, regions slightly outside of the film plane will also be sampled, which
       improves the image quality at the edges especially with large reconstruction filters. */
    m_high_quality_edges = props.bool_("high_quality_edges", false);

    // Use the provided reconstruction filter, if any.
    for (auto &kv : props.objects()) {
        auto *rfilter = dynamic_cast<ReconstructionFilter *>(kv.second.get());
        if (rfilter) {
            if (m_filter)
                Throw("A film can only have one reconstruction filter.");
            m_filter = rfilter;
        } else {
            Throw("Tried to add an unsupported object of type %s", kv.second);
        }
    }

    if (!m_filter) {
        // No reconstruction filter has been selected. Load a Gaussian filter by default
        m_filter = static_cast<ReconstructionFilter *>(
            PluginManager::instance()->create_object<ReconstructionFilter>(Properties("gaussian")));
    }
}

template <typename Float, typename Spectrum>
Film<Float, Spectrum>::~Film() {}

template <typename Float, typename Spectrum>
void Film<Float, Spectrum>::set_crop_window(const Vector2i &crop_size, const Point2i &crop_offset) {
    m_crop_size   = crop_size;
    m_crop_offset = crop_offset;
    check_valid_crop_window();
}

template <typename Float, typename Spectrum>
void Film<Float, Spectrum>::check_valid_crop_window() const {
    if (m_crop_offset.x() < 0 || m_crop_offset.y() < 0 || m_crop_size.x() <= 0 ||
        m_crop_size.y() <= 0 || m_crop_offset.x() + m_crop_size.x() > m_size.x() ||
        m_crop_offset.y() + m_crop_size.y() > m_size.y()) {
        Throw("Invalid crop window specification!\n"
              "offset %s + crop size %s vs full size %s",
              m_crop_offset, m_crop_size, m_size);
    }
}

MTS_INSTANTIATE_OBJECT(Film)
NAMESPACE_END(mitsuba)
