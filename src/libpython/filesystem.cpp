#include "python.h"
#include <mitsuba/core/filesystem.h>

using namespace mitsuba::filesystem;

MTS_PY_EXPORT(filesystem) {
  auto m2 = m.def_submodule("filesystem", "Lightweight cross-platform filesystem utilities");

  // TODO: python bindings

  py::class_<path>(m2, "path", DM(filesystem, path))
      // TODO: overloaded constructors
      .def(py::init<>(), DM(filesystem, path, path))
      .def("empty", &path::empty, DM(filesystem, path, empty))
      .def("is_absolute", &path::is_absolute, DM(filesystem, path, is_absolute))
      .def("is_relative", &path::is_relative, DM(filesystem, path, is_relative))
      .def("parent_path", &path::parent_path, DM(filesystem, path, parent_path))
      .def("extension", &path::extension, DM(filesystem, path, extension))
      .def("filename", &path::filename, DM(filesystem, path, filename))
      .def("native", &path::native, DM(filesystem, path, native));
      // TODO: operator string_type() const
      // TODO: operator overloading

//  m2.def("current_path", &current_path, "hello"); // DM(filesystem, current_path)
}
