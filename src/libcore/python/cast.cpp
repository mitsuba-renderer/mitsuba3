#include <mitsuba/python/python.h>
#include <mitsuba/core/logger.h>

using Caster = py::object(*)(mitsuba::Object *, py::handle parent);

static std::vector<void *> casters;

py::object cast_object(Object *o, py::handle parent) {
    for (auto caster : casters) {
        py::object po = ((Caster) caster)(o, parent);
        if (po)
            return po;
    }

    py::return_value_policy rvp = parent ? py::return_value_policy::reference_internal
                                         : py::return_value_policy::take_ownership;

    return py::cast(o, rvp, parent);
}

MTS_PY_EXPORT(Cast) {
    m.attr("casters") = py::cast((void *) &casters);
}