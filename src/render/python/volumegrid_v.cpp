#include <mitsuba/render/volumegrid.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/ndarray.h>

MI_PY_EXPORT(VolumeGrid) {
    MI_PY_IMPORT_TYPES(VolumeGrid)
    MI_PY_CLASS(VolumeGrid, Object).def("__init__",
            [](nb::ndarray<ScalarFloat, nb::c_contig, nb::device::cpu> &obj, bool compute_max = true) {
            if (obj.ndim() != 3 && obj.ndim() != 4)
                throw nb::type_error("Expected an array of size 3 or 4");

            size_t channel_count = 1;
            if (obj.ndim() == 4)
                channel_count = obj.shape(3);

            ScalarVector3u size((uint32_t) obj.shape(2),
                                (uint32_t) obj.shape(1),
                                (uint32_t) obj.shape(0));
            auto volumegrid = new VolumeGrid(size, (uint32_t) channel_count);
            memcpy(volumegrid->data(), obj.data(), volumegrid->buffer_size());

            ScalarFloat max = 0.f;
            std::vector<ScalarFloat> max_per_channel(channel_count, -dr::Infinity<ScalarFloat>);
            if (compute_max) {
                size_t ch_index = 0;
                for (size_t i = 0; i < dr::prod(size) * channel_count; ++i) {
                    max = dr::maximum(max, volumegrid->data()[i]);
                    ch_index = i % channel_count;
                    max_per_channel[ch_index] = dr::maximum(max_per_channel[ch_index], volumegrid->data()[i]);
                }
            }

            volumegrid->set_max(max);
            volumegrid->set_max_per_channel(max_per_channel.data());
            return volumegrid;
        })
        .def_method(VolumeGrid, size)
        .def_method(VolumeGrid, channel_count)
        .def_method(VolumeGrid, max)
        .def("max_per_channel",
            [] (const VolumeGrid *volgrid) {
                std::vector<ScalarFloat> max_values(volgrid->channel_count());
                volgrid->max_per_channel(max_values.data());
                return max_values;
            },
            D(VolumeGrid, max_per_channel))
        .def_method(VolumeGrid, set_max)
        .def("set_max_per_channel",
            [] (VolumeGrid *volgrid, std::vector<ScalarFloat> &max_values) {
                volgrid->set_max_per_channel(max_values.data());
            },
            D(VolumeGrid, set_max_per_channel))
        .def_method(VolumeGrid, bytes_per_voxel)
        .def_method(VolumeGrid, buffer_size)
        .def("write", nb::overload_cast<Stream *>(&VolumeGrid::write, nb::const_),
            "stream"_a, D(VolumeGrid, write), nb::call_guard<nb::gil_scoped_release>())
        .def("write", nb::overload_cast<const fs::path &>(
                &VolumeGrid::write, nb::const_), "path"_a, D(VolumeGrid, write, 2),
                nb::call_guard<nb::gil_scoped_release>())

        .def(nb::init<const fs::path &>(), "path"_a,
            nb::call_guard<nb::gil_scoped_release>())
        .def(nb::init<Stream *>(), "stream"_a,
            nb::call_guard<nb::gil_scoped_release>())

        .def_prop_ro("__array_interface__", [](VolumeGrid &grid) -> nb::object {
            nb::dict result;
            auto size = grid.size();
            if (grid.channel_count() == 1)
                result["shape"] = nb::make_tuple(size.z(), size.y(), size.x());
            else
                result["shape"] = nb::make_tuple(size.z(), size.y(), size.x(), grid.channel_count());

            std::string code(3, '\0');
            #if defined(LITTLE_ENDIAN)
                code[0] = '<';
            #else
                code[0] = '>';
            #endif
            code[1] = 'f';
            code[2] = (char) ('0' + sizeof(ScalarFloat));
            #if PY_MAJOR_VERSION > 3
                result["typestr"] = code;
            #else
                result["typestr"] = nb::bytes(code.data(), code.length());
            #endif

            result["data"] = nb::make_tuple(size_t(grid.data()), false);
            result["version"] = 3;
            return nb::object(result);
        });
}
