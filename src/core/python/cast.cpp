#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

using Caster = py::object(*)(mitsuba::Object *);

static std::vector<void *> casters;

py::object cast_object(Object *o) {
    for (auto &caster : casters) {
        py::object po = ((Caster) caster)(o);
        if (po)
            return po;
    }
    return py::cast(o);
}

MTS_PY_EXPORT(Cast) {
    m.attr("casters") = py::cast((void *) &casters);
    m.attr("cast_object") = py::cast((void *) &cast_object);
}
