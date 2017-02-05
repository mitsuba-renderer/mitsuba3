#include <mitsuba/core/ray.h>
#include "python.h"

template <typename Type> void bind_ray(py::module &m, const char *name) {
    typedef typename Type::PointType PointType;
    typedef typename Type::VectorType VectorType;
    typedef typename Type::Scalar Scalar;

    py::class_<Type>(m, name, D(TRay))
        .def(py::init<>(), D(TRay, TRay))
        .def(py::init<const Type &>(), D(TRay, TRay))
        .def(py::init<PointType, VectorType>(), D(TRay, TRay, 2))
        .def(py::init<PointType, VectorType, Scalar, Scalar>(), D(TRay, TRay, 3))
        .def(py::init<const Type &, Scalar, Scalar>(), D(TRay, TRay, 4))
        .def("update", &Type::update, D(TRay, update))
        .def("reverse", &Type::reverse, D(TRay, reverse))
        .def("__call__", &Type::operator(), D(TRay, operator, call))
        .def_readwrite("o", &Type::o, D(TRay, o))
        .def_readwrite("d", &Type::d, D(TRay, d))
        .def_readwrite("d_rcp", &Type::d_rcp, D(TRay, d_rcp))
        .def_readwrite("mint", &Type::mint, D(TRay, mint))
        .def_readwrite("maxt", &Type::maxt, D(TRay, maxt));
}

MTS_PY_EXPORT(Ray) {
    bind_ray<Ray3f>(m, "Ray3f");
}

