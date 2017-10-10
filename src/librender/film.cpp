#include <mitsuba/render/film.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

Film::Film(const Properties &props) : Object() {
    bool is_m_film = string::to_lower(props.plugin_name()) == "mfilm";

    // Horizontal and vertical film resolution in pixels
    m_size = Vector2i(
        props.int_("width", is_m_film ? 1 : 768),
        props.int_("height", is_m_film ? 1 : 576)
    );
    // Crop window specified in pixels - by default, this matches the full
    // sensor area.
    m_crop_offset = Point2i(
        props.int_("crop_offset_x", 0),
        props.int_("crop_offset_y", 0)
    );
    m_crop_size = Vector2i(
        props.int_("crop_width", m_size.x()),
        props.int_("crop_height", m_size.y())
    );
    if (m_crop_offset.x() < 0 || m_crop_offset.y() < 0 ||
        m_crop_size.x() <= 0 || m_crop_size.y() <= 0 ||
        m_crop_offset.x() + m_crop_size.x() > m_size.x() ||
        m_crop_offset.y() + m_crop_size.y() > m_size.y() ) {
        Log(EError, "Invalid crop window specification!");
    }

    /* If set to true, regions slightly outside of the film
       plane will also be sampled, which improves the image
       quality at the edges especially with large reconstruction
       filters. */
    m_high_quality_edges = props.bool_("high_quality_edges", false);

    configure();
}

void Film::configure() {
    if (m_filter == nullptr) {
        // No reconstruction filter has been selected. Load a Gaussian filter by default
        m_filter = static_cast<ReconstructionFilter *>(
            PluginManager::instance()->create_object<ReconstructionFilter>(
                Properties("gaussian"))
        );
    }
}

MTS_IMPLEMENT_CLASS(Film, Object);
NAMESPACE_END(mitsuba)
