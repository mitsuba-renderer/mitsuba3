#include <mitsuba/core/fresolver.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(FileResolver) {
    MTS_PY_CLASS(FileResolver, Object)
        .def(py::init<>(), D(FileResolver, FileResolver))
        .def(py::init<const FileResolver &>(), D(FileResolver, FileResolver, 2))
        .def("__len__", &FileResolver::size, D(FileResolver, size))
        .def("__iter__", [](const FileResolver &fr) { return py::make_iterator(fr.begin(), fr.end()); },
                        py::keep_alive<0, 1>())
        .def("__delitem__", [](FileResolver &fr, size_t i) {
            if (i >= fr.size())
                throw py::index_error();
            fr.erase(fr.begin() + i);
        })
        .def("__getitem__", [](const FileResolver &fr, size_t i) -> fs::path {
            if (i >= fr.size())
                throw pybind11::index_error();
            return fr[i];
        })
        .def("__setitem__", [](FileResolver &fr, size_t i, const fs::path &value) {
            if (i >= fr.size())
                throw pybind11::index_error();
            fr[i] = value;
        })
        .def_method(FileResolver, resolve)
        .def_method(FileResolver, clear)
        .def_method(FileResolver, prepend)
        .def_method(FileResolver, append);
}
