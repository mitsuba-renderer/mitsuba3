#include "python.h"
#include <mitsuba/core/filesystem.h>

using namespace mitsuba::filesystem;

MTS_PY_EXPORT(filesystem) {
  auto m2 = m.def_submodule("filesystem", "Lightweight cross-platform filesystem utilities");

  // TODO: python bindings

  py::class_<path>(m2, "path", DM(filesystem, path))
      .def(py::init<>(), DM(filesystem, path, path))
      .def(py::init<const path &>(), DM(filesystem, path, path, 2))
      .def(py::init<const char *>(), DM(filesystem, path, path, 4))
      .def(py::init<const string_type &>(), DM(filesystem, path, path, 5))

      .def("clear", &path::clear, DM(filesystem, path, clear))
      .def("empty", &path::empty, DM(filesystem, path, empty))
      .def("is_absolute", &path::is_absolute, DM(filesystem, path, is_absolute))
      .def("is_relative", &path::is_relative, DM(filesystem, path, is_relative))
      .def("parent_path", &path::parent_path, DM(filesystem, path, parent_path))
      .def("extension", &path::extension, DM(filesystem, path, extension))
      .def("filename", &path::filename, DM(filesystem, path, filename))
      .def("native", &path::native, DM(filesystem, path, native))
      .def(py::self / py::self, DM(filesystem, path, operator_div))
      // Note: python doesn't allow for overload of assignment operator
      .def(py::self == py::self, DM(filesystem, path, operator_eq))
      .def(py::self != py::self, DM(filesystem, path, operator_ne))
      .def("__str__", &path::native, DM(filesystem, path, native))
      .def("__repr__", &path::native, DM(filesystem, path, native));
      // TODO: protected methods?

  m2.def("current_path", &current_path, DM(filesystem, current_path));
  m2.def("make_absolute", &make_absolute, DM(filesystem, make_absolute));
  m2.def("is_regular_file", &is_regular_file, DM(filesystem, is_regular_file));
  m2.def("is_directory", &is_directory, DM(filesystem, is_directory));
  m2.def("exists", &exists, DM(filesystem, exists));
  m2.def("create_directory", &create_directory, DM(filesystem, create_directory));
  m2.def("resize_file", &resize_file, DM(filesystem, resize_file));
  m2.def("remove", &filesystem::remove, DM(filesystem, remove));
}
