#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/mmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/tensor.h>
#include <mitsuba/python/python.h>

#include <nanobind/ndarray.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/string.h>

#include <drjit/tensor.h>
#include <drjit/python.h>

MI_PY_EXPORT(MemoryMappedFile) {
    MI_PY_CLASS(MemoryMappedFile, Object)
        .def(nb::init<const fs::path &, size_t>(),
            D(MemoryMappedFile, MemoryMappedFile), "filename"_a, "size"_a)
        .def(nb::init<const fs::path &, bool>(),
            D(MemoryMappedFile, MemoryMappedFile, 2), "filename"_a, "write"_a = false)
        .def("__init__", [](MemoryMappedFile* t,
            const fs::path &p,
            nb::ndarray<nb::device::cpu> array) {
            size_t size = array.size() * array.itemsize();
            auto m = new (t) MemoryMappedFile(p, size);
            memcpy(m->data(), array.data(), size);
        }, "filename"_a, "array"_a)
        .def("size", &MemoryMappedFile::size, D(MemoryMappedFile, size))
        .def("data", nb::overload_cast<>(&MemoryMappedFile::data), D(MemoryMappedFile, data))
        .def("resize", &MemoryMappedFile::resize, D(MemoryMappedFile, resize))
        .def("filename", &MemoryMappedFile::filename, D(MemoryMappedFile, filename))
        .def("can_write", &MemoryMappedFile::can_write, D(MemoryMappedFile, can_write))
        .def_static("create_temporary", &MemoryMappedFile::create_temporary, D(MemoryMappedFile, create_temporary))
        .def("__array__", [](MemoryMappedFile &m) {
            return nb::ndarray<nb::numpy, uint8_t>((uint8_t*) m.data(), { m.size() }, nb::handle());
        }, nb::rv_policy::reference_internal);

    auto tf = MI_PY_CLASS(TensorFile, MemoryMappedFile)
        .def(nb::init<fs::path>())
        .def("__contains__", &TensorFile::has_field)
        .def("__getitem__", &TensorFile::field, nb::rv_policy::reference_internal);

    nb::class_<TensorFile::Field>(tf, "Field")
        .def_ro("dtype", &TensorFile::Field::dtype)
        .def_ro("offset", &TensorFile::Field::offset)
        .def_ro("shape", &TensorFile::Field::shape)
        .def("__repr__", &TensorFile::Field::to_string)
        .def("to", [](TensorFile::Field &f, nb::type_object_t<dr::ArrayBase> tp) -> nb::object {
            const auto &s = nb::type_supplement<dr::ArraySupplement>(tp);
            if (!s.is_tensor)
                nb::raise_type_error("to(): 'dtype' must be a Dr.Jit tensor type!");

            nb::object tensor = nb::inst_alloc_zero(tp);
            nb::object array = nb::steal(s.tensor_array(tensor.ptr()));
            dr::vector<size_t> &shape = s.tensor_shape(nb::inst_ptr<dr::ArrayBase>(tensor));

            using Type = Struct::Type;
            VarType vt;
            switch (f.dtype) {
                case Type::UInt32: vt = VarType::UInt32; break;
                case Type::Int32: vt = VarType::Int32; break;
                case Type::UInt64: vt = VarType::UInt64; break;
                case Type::Int64: vt = VarType::Int64; break;
                case Type::Float16: vt = VarType::Float16; break;
                case Type::Float32: vt = VarType::Float32; break;
                case Type::Float64: vt = VarType::Float64; break;
                default: vt = VarType::Void;
            }

            if (vt != (VarType) s.type)
                nb::raise_type_error("to(): incompatible dtype (got %s, field has type %s)",
                                     jit_type_name((VarType) s.type),
                                     jit_type_name(vt));

            size_t size = 1;
            shape.reserve(f.shape.size());
            for (size_t d : f.shape) {
                size *= d;
                shape.push_back(d);
            }

            const auto &s2 = nb::type_supplement<dr::ArraySupplement>(s.array);
            s2.init_data(size, f.data, nb::inst_ptr<dr::ArrayBase>(array));

            return tensor;
        }, "dtype"_a);
}
