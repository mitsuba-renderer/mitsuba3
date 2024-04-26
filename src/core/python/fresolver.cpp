#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/core/fresolver.h>
#include <mitsuba/python/python.h>

#include <nanobind/make_iterator.h>

MI_PY_EXPORT(FileResolver) {
    MI_PY_CLASS(FileResolver, Object)
        .def(nb::init<>(), D(FileResolver, FileResolver))
        .def(nb::init<const FileResolver &>(), D(FileResolver, FileResolver, 2))
        .def("__len__", &FileResolver::size, D(FileResolver, size))
        .def("__iter__", [](const FileResolver &fr) { return nb::make_iterator(
            nb::type<FileResolver>(), "fr_iterator", fr.begin(), fr.end()); },
            nb::keep_alive<0, 1>())
        .def("__delitem__", [](FileResolver &fr, size_t i) {
            if (i >= fr.size())
                throw nb::index_error();
            fr.erase(fr.begin() + i);
        })
        .def("__getitem__", [](const FileResolver &fr, size_t i) -> fs::path {
            if (i >= fr.size())
                throw nb::index_error();
            return fr[i];
        })
        .def("__setitem__", [](FileResolver &fr, size_t i, const fs::path &value) {
            if (i >= fr.size())
                throw nb::index_error();
            fr[i] = value;
        })
        .def_method(FileResolver, resolve)
        .def_method(FileResolver, clear)
        .def_method(FileResolver, prepend)
        .def_method(FileResolver, append);
}
