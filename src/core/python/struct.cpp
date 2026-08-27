#include <mitsuba/python/python.h>
#include <struct-jit/python.h>

/// Ship struct-jit's own nanobind bindings as part of Mitsuba
MI_PY_EXPORT(Struct) {
    nb::module_ sjm = m.def_submodule("_struct_jit");

    // Temporarily change the module name (for pydoc and stub generation)
    sjm.attr("__name__") = "mitsuba";
    struct_jit::python_export(sjm);
    sjm.attr("__name__") = "mitsuba._struct_jit";

    nb::object Struct = sjm.attr("Struct");

    // Nest the enums and fields under Struct (historical Mitsuba layout; note
    // the legacy plural 'Flags').
    auto nest = [&](const char *from, const char *to) {
        nb::object o = sjm.attr(from);
        o.attr("__name__") = to;
        o.attr("__qualname__") = nb::str("Struct.{}").format(to);
        Struct.attr(to) = o;
    };

    nest("Type", "Type");
    nest("Flag", "Flags");
    nest("ByteOrder", "ByteOrder");
    nest("Field", "Field");

    nb::object Converter = sjm.attr("Converter");
    Converter.attr("__name__") = "StructConverter";
    Converter.attr("__qualname__") = "StructConverter";

    m.attr("Struct")          = Struct;
    m.attr("StructConverter") = Converter;
}
