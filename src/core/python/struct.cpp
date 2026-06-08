#include <mitsuba/python/python.h>
#include <struct-jit/python.h>

/// Ship struct-jit's own nanobind bindings as part of Mitsuba
MI_PY_EXPORT(Struct) {
    nb::module_ sjm = m.def_submodule("_struct_jit");
    struct_jit::python_export(sjm);

    nb::object Struct = sjm.attr("Struct");

    // Nest the enums under Struct (historical Mitsuba layout; note the legacy
    // plural 'Flags').
    Struct.attr("Type")      = sjm.attr("Type");
    Struct.attr("Flags")     = sjm.attr("Flag");
    Struct.attr("ByteOrder") = sjm.attr("ByteOrder");

    m.attr("Struct")          = Struct;
    m.attr("StructConverter") = sjm.attr("Converter");
}
