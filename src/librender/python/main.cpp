#include <mitsuba/python/python.h>

PYBIND11_MODULE(render_ext, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.render";


    // Change module name back to correct value
    m.attr("__name__") = "mitsuba.render_ext";
}
