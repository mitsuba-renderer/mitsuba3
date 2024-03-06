#include <mitsuba/core/struct.h>
#include <mitsuba/core/simd.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

MI_PY_EXPORT(Struct) {
    auto c = MI_PY_CLASS(Struct, Object);
    nb::class_<Struct::Field> field(c, "Field", D(Struct, Field));

    nb::enum_<Struct::Type>(c, "Type")
        .value("Int8",    Struct::Type::Int8,    D(Struct, Type, Int8))
        .value("UInt8",   Struct::Type::UInt8,   D(Struct, Type, UInt8))
        .value("Int16",   Struct::Type::Int16,   D(Struct, Type, Int16))
        .value("UInt16",  Struct::Type::UInt16,  D(Struct, Type, UInt16))
        .value("Int32",   Struct::Type::Int32,   D(Struct, Type, Int32))
        .value("UInt32",  Struct::Type::UInt32,  D(Struct, Type, UInt32))
        .value("Int64",   Struct::Type::Int64,   D(Struct, Type, Int64))
        .value("UInt64",  Struct::Type::UInt64,  D(Struct, Type, UInt64))
        .value("Float16", Struct::Type::Float16, D(Struct, Type, Float16))
        .value("Float32", Struct::Type::Float32, D(Struct, Type, Float32))
        .value("Float64", Struct::Type::Float64, D(Struct, Type, Float64))
        .value("Invalid", Struct::Type::Invalid, D(Struct, Type, Invalid));

    nb::enum_<Struct::Flags>(c, "Flags", nb::is_arithmetic())
        .value("Empty",      Struct::Flags::Empty, D(Struct, Flags, Empty))
        .value("Normalized", Struct::Flags::Normalized, D(Struct, Flags, Normalized))
        .value("Gamma",      Struct::Flags::Gamma,   D(Struct, Flags, Gamma))
        .value("Weight",     Struct::Flags::Weight,  D(Struct, Flags, Weight))
        .value("Assert",     Struct::Flags::Assert,  D(Struct, Flags, Assert))
        .value("Alpha",     Struct::Flags::Alpha,  D(Struct, Flags, Alpha))
        .value("PremultipliedAlpha",     Struct::Flags::PremultipliedAlpha,  D(Struct, Flags, PremultipliedAlpha))
        .value("Default",    Struct::Flags::Default, D(Struct, Flags, Default));

    nb::enum_<Struct::ByteOrder>(c, "ByteOrder")
        .value("LittleEndian", Struct::ByteOrder::LittleEndian,
            D(Struct, ByteOrder, LittleEndian))
        .value("BigEndian", Struct::ByteOrder::BigEndian,
            D(Struct, ByteOrder, BigEndian))
        .value("HostByteOrder", Struct::ByteOrder::HostByteOrder,
            D(Struct, ByteOrder, HostByteOrder));

    c.def(nb::init<bool, Struct::ByteOrder>(), "pack"_a = false,
            "byte_order"_a = Struct::ByteOrder::HostByteOrder,
            D(Struct, Struct))
        .def("append",
            (Struct &(Struct::*)(const std::string&, Struct::Type, uint32_t, double)) &Struct::append,
            "name"_a, "type"_a, "flags"_a = Struct::Flags::Empty, "default"_a = 0.0,
            D(Struct, append), nb::rv_policy::none)
        .def("field", nb::overload_cast<const std::string &>(&Struct::field), D(Struct, field),
            nb::rv_policy::reference_internal)
        .def("__getitem__", [](Struct &s, size_t i) -> Struct::Field& {
            if (i >= s.field_count())
                throw nb::index_error();
            return s[i];
        }, nb::rv_policy::reference_internal)
        .def("__len__", &Struct::field_count)
        .def(nb::self == nb::self)
        .def(nb::self != nb::self)
        .def("__hash__", [](const Struct &s) { return hash(s); })
        .def_method(Struct, size)
        .def_method(Struct, alignment)
        .def_method(Struct, byte_order)
        .def_method(Struct, field_count)
        .def_method(Struct, has_field)
        .def_static("is_float", &Struct::is_float, D(Struct, is_float))
        .def_static("is_integer", &Struct::is_integer, D(Struct, is_integer))
        .def_static("is_signed", &Struct::is_signed, D(Struct, is_signed))
        .def_static("is_unsigned", &Struct::is_unsigned, D(Struct, is_unsigned))
        .def_static("range", &Struct::range, D(Struct, range));

    field.def("is_float", &Struct::Field::is_float, D(Struct, Field, is_float))
        .def("is_integer", &Struct::Field::is_integer, D(Struct, Field, is_integer))
        .def("is_signed", &Struct::Field::is_signed, D(Struct, Field, is_signed))
        .def("is_unsigned", &Struct::Field::is_unsigned, D(Struct, Field, is_unsigned))
        .def("range", &Struct::Field::range, D(Struct, Field, range))
        .def(nb::self == nb::self)
        .def(nb::self != nb::self)
        .def("__hash__", [](const Struct::Field &f) { return hash(f); })
        .def_rw("type", &Struct::Field::type, D(Struct, Field, type))
        .def_rw("size", &Struct::Field::size, D(Struct, Field, size))
        .def_rw("offset", &Struct::Field::offset, D(Struct, Field, offset))
        .def_rw("flags", &Struct::Field::flags, D(Struct, Field, flags))
        .def_rw("name", &Struct::Field::name, D(Struct, Field, name))
        .def_rw("blend", &Struct::Field::blend, D(Struct, Field, blend));

    MI_PY_CLASS(StructConverter, Object)
        .def(nb::init<const Struct *, const Struct *, bool>(), "source"_a, "target"_a, "dither"_a = false)
        .def_method(StructConverter, source)
        .def_method(StructConverter, target)
        .def("convert", [](const StructConverter &c, nb::bytes input_) -> nb::bytes {
            std::string input(input_.c_str(), input_.size());
            size_t count = input.length() / c.source()->size();
            size_t output_size = c.target()->size() * count;
            std::string result(c.target()->size() * count, '\0');
            if (!c.convert(count, input.data(), (void *) result.data()))
                throw std::runtime_error("Conversion failed!");

            return nb::bytes(result.c_str(), output_size);
        });
}
