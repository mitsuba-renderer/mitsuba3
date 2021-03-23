#include <mitsuba/render/volumegrid.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <pybind11/numpy.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(VolumeGrid) {
    using Float = typename VolumeGrid::Float;
    MTS_IMPORT_CORE_TYPES()

    auto volumegrid = MTS_PY_CLASS(VolumeGrid, Object);
    volumegrid.def(py::init([](py::array obj, bool compute_max=true) {
            if (!obj.dtype().is(py::dtype("f")))
                throw py::type_error("Expected floating point (float32) array");

            if (obj.ndim() != 3 && obj.ndim() != 4)
                throw py::type_error("Expected an array of size 3 or 4");

            size_t channel_count = 1;
            if (obj.ndim() == 4) {
                channel_count = obj.shape()[3];
            }

            obj = py::array::ensure(obj, py::array::c_style);
            ScalarVector3i size(obj.shape()[2], obj.shape()[1], obj.shape()[0]);
            auto volumegrid = new VolumeGrid(size, channel_count);
            memcpy(volumegrid->data(), obj.data(), volumegrid->buffer_size());

            ScalarFloat max = 0.f;
            if (compute_max)
                for (size_t i = 0; i < ek::hprod(size) * channel_count; ++i)
                    max = ek::max(max, volumegrid->data()[i]);
            volumegrid->set_max(max);
            return volumegrid;
        }), "array"_a, "compute_max"_a = true, "Initialize a VolumeGrid from a NumPy array")

        .def_method(VolumeGrid, size)
        .def_method(VolumeGrid, channel_count)
        .def_method(VolumeGrid, max)
        .def_method(VolumeGrid, set_max)
        .def_method(VolumeGrid, bytes_per_voxel)
        .def_method(VolumeGrid, buffer_size)
        .def("write", py::overload_cast<Stream *>(&VolumeGrid::write, py::const_),
            "stream"_a, D(VolumeGrid, write), py::call_guard<py::gil_scoped_release>())
        .def("write", py::overload_cast<const fs::path &>(
                &VolumeGrid::write, py::const_), "path"_a, D(VolumeGrid, write, 2),
                py::call_guard<py::gil_scoped_release>())

        .def(py::init<const fs::path &>(), "path"_a,
            py::call_guard<py::gil_scoped_release>())
        .def(py::init<Stream *>(), "stream"_a,
            py::call_guard<py::gil_scoped_release>())

        .def_property_readonly("__array_interface__", [](VolumeGrid &grid) -> py::object {
            py::dict result;
            auto size = grid.size();
            if (grid.channel_count() == 1)
                result["shape"] = py::make_tuple(size.z(), size.y(), size.x());
            else
                result["shape"] = py::make_tuple(size.z(), size.y(), size.x(), grid.channel_count());

            std::string code(3, '\0');
            #if defined(LITTLE_ENDIAN)
                code[0] = '<';
            #else
                code[0] = '>';
            #endif
            code[1] = 'f';
            code[2] = (char) ('0' + sizeof(float));
            #if PY_MAJOR_VERSION > 3
                result["typestr"] = code;
            #else
                result["typestr"] = py::bytes(code);
            #endif

            result["data"] = py::make_tuple(size_t(grid.data()), false);
            result["version"] = 3;
            return py::object(result);
        });
}
