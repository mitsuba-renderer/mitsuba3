#include <mitsuba/python/python.h>

MI_PY_DECLARE(BSDFContext);
MI_PY_DECLARE(EmitterExtras);
MI_PY_DECLARE(RayFlags);
MI_PY_DECLARE(MicrofacetType);
MI_PY_DECLARE(PhaseFunctionExtras);
MI_PY_DECLARE(Spiral);
MI_PY_DECLARE(Sensor);
MI_PY_DECLARE(VolumeGrid);
MI_PY_DECLARE(FilmFlags);

PYBIND11_MODULE(render_ext, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.render";

    MI_PY_IMPORT(BSDFContext);
    MI_PY_IMPORT(EmitterExtras);
    MI_PY_IMPORT(RayFlags);
    MI_PY_IMPORT(MicrofacetType);
    MI_PY_IMPORT(PhaseFunctionExtras);
    MI_PY_IMPORT(Spiral);
    MI_PY_IMPORT(Sensor);
    MI_PY_IMPORT(FilmFlags);

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba.render_ext";
}
