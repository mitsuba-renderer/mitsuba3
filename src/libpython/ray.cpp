#include <mitsuba/core/ray.h>
#include "python.h"

template <typename Type> void bind_ray(py::module &m, const char *name) {
    typedef typename Type::PointType PointType;
    typedef typename Type::VectorType VectorType;
    typedef typename Type::Scalar Scalar;

    py::class_<Type>(m, name, DM(TRay))
        .def(py::init<>(), DM(TRay, TRay))
        .def(py::init<const Type &>(), DM(TRay, TRay))
        .def(py::init<PointType, VectorType>(), DM(TRay, TRay, 2))
        .def(py::init<PointType, VectorType, Scalar, Scalar>(), DM(TRay, TRay, 3))
        .def(py::init<const Type &, Scalar, Scalar>(), DM(TRay, TRay, 4))
        .def("update", &Type::update, DM(TRay, update))
        .def("reverse", &Type::reverse, DM(TRay, reverse))
        .def("__call__", &Type::operator(), DM(TRay, operator, call))
        .def_readwrite("o", &Type::o, DM(TRay, o))
        .def_readwrite("d", &Type::d, DM(TRay, d))
        .def_readwrite("dRcp", &Type::dRcp, DM(TRay, dRcp))
        .def_readwrite("mint", &Type::mint, DM(TRay, mint))
        .def_readwrite("maxt", &Type::maxt, DM(TRay, maxt));
}

MTS_PY_EXPORT(Ray) {
    bind_ray<Ray3f>(m, "Ray3f");
}

