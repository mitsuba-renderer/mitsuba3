#include <mitsuba/python/python.h>

#if defined(MTS_ENABLE_OPTIX)
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/optix/shapes.h>
#endif

MTS_PY_DECLARE(BSDFContext);
MTS_PY_DECLARE(EmitterExtras);
MTS_PY_DECLARE(MicrofacetType);
MTS_PY_DECLARE(PhaseFunctionExtras);
MTS_PY_DECLARE(Spiral);

PYBIND11_MODULE(render_ext, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.render";

#if defined(MTS_ENABLE_OPTIX)
    optix_initialize();
#endif

    MTS_PY_IMPORT(BSDFContext);
    MTS_PY_IMPORT(EmitterExtras);
    MTS_PY_IMPORT(MicrofacetType);
    MTS_PY_IMPORT(PhaseFunctionExtras);
    MTS_PY_IMPORT(Spiral);

#if defined(MTS_ENABLE_OPTIX)
    /* Register a cleanup callback function that is invoked when
       the 'mitsuba::BSDFContext' Python type is garbage collected */
    py::cpp_function cleanup_callback(
        [](py::handle weakref) {
            optix_shutdown();
            weakref.dec_ref();
        }
    );

    (void) py::weakref(m.attr("BSDFContext"), cleanup_callback).release();
#endif 

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba.render_ext";
}
