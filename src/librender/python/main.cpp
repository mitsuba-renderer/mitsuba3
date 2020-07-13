#include <mitsuba/python/python.h>

MTS_PY_DECLARE(BSDFContext);
MTS_PY_DECLARE(EmitterExtras);
MTS_PY_DECLARE(HitComputeFlags);
MTS_PY_DECLARE(MicrofacetType);
MTS_PY_DECLARE(PhaseFunctionExtras);
MTS_PY_DECLARE(Spiral);

PYBIND11_MODULE(render_ext, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.render";

    MTS_PY_IMPORT(BSDFContext);
    MTS_PY_IMPORT(EmitterExtras);
    MTS_PY_IMPORT(HitComputeFlags);
    MTS_PY_IMPORT(MicrofacetType);
    MTS_PY_IMPORT(PhaseFunctionExtras);
    MTS_PY_IMPORT(Spiral);

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba.render_ext";
}
