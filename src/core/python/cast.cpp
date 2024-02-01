#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

using Caster = nb::object(*)(mitsuba::Object *);

static std::vector<void *> casters;

nb::object cast_object(Object *o) {
    for (auto &caster : casters) {
        nb::object po = ((Caster) caster)(o);
        if (po)
            return po;
    }
    return nb::cast(o);
}

MI_PY_EXPORT(Cast) {
    m.attr("casters") = nb::cast((void *) &casters);
    m.attr("cast_object") = nb::cast((void *) &cast_object);
}
