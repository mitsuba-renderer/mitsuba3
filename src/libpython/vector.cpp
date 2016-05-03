#include <mitsuba/core/vector.h>
#include "python.h"

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)

/* Dynamic matrices */
template <typename Type, typename std::enable_if<!Type::IsVectorAtCompileTime && Type::SizeAtCompileTime == Eigen::Dynamic, int> = 0>
void bind_dynamic(py::class_<Type> &type) {
    typedef typename Type::Scalar Scalar;

    /* Dynamic matrix */
    type.def(py::init<size_t, size_t>())
        .def("__init__", [](Type &m, py::array_t<Scalar> b) {
            py::buffer_info info = b.request();
            if (info.ndim == 1) {
                new (&m) Type(info.shape[0], 1);
                memcpy(m.data(), info.ptr, sizeof(Scalar) * m.size());
            } else if (info.ndim == 2) { 
                if (info.strides[0] == sizeof(Scalar)) {
                    new (&m) Type(info.shape[0], info.shape[1]);
                    memcpy(m.data(), info.ptr, sizeof(Scalar) * m.size());
                    if (!(Type::Flags & Eigen::RowMajorBit))
                        m.transposeInPlace();
                } else {
                    new (&m) Type(info.shape[1], info.shape[0]);
                    memcpy(m.data(), info.ptr, sizeof(Scalar) * m.size());
                    if (Type::Flags & Eigen::RowMajorBit)
                        m.transposeInPlace();
                }
            } else {
                throw std::runtime_error("Incompatible buffer dimension!");
            }
        })
        .def("resize", [](Type &m, size_t s0, size_t s1) { m.resize(s0, s1); })
        .def("conservativeResize", [](Type &m, size_t s0, size_t s1) { m.conservativeResize(s0, s1); })
        .def("resizeLike", [](Type &m, const Type &m2) { m.resizeLike(m2); })
        .def("transposeInPlace", [](Type &m) { m.transposeInPlace(); })
        .def("transpose", [](Type &m) -> Type { return m.transpose(); })
        /* Accessors */
        .def("__getitem__", [](const Type &m, std::pair<size_t, size_t> i) {
            if (i.first >= (size_t) m.rows() || i.second >= (size_t) m.cols())
                throw py::index_error();
            return m(i.first, i.second);
         })
        .def("__setitem__", [](Type &m, std::pair<size_t, size_t> i, Scalar v) {
            if (i.first >= (size_t) m.rows() || i.second >= (size_t) m.cols())
                throw py::index_error();
            m(i.first, i.second) = v;
         })
        /* Static initializers */
        .def_static("Zero", [](size_t n, size_t m) -> Type { return Type::Zero(n, m); })
        .def_static("Ones", [](size_t n, size_t m) -> Type { return Type(Type::Ones(n, m)); })
        .def_static("Constant", [](size_t n, size_t m, Scalar value) -> Type { return Type::Constant(n, m, value); })
        .def_static("Identity", [](size_t n, size_t m) ->Type { return Type::Identity(n, m); });
}


/* Dynamic vectors */
template <typename Type, typename std::enable_if<Type::IsVectorAtCompileTime && Type::SizeAtCompileTime == Eigen::Dynamic, int> = 0>
void bind_dynamic(py::class_<Type> &type) {
    typedef typename Type::Scalar Scalar;

    /* Dynamic vector */
    type.def(py::init<size_t>())
        .def("__init__", [](Type &m, py::array_t<Scalar> b) {
            py::buffer_info info = b.request();
            if ( info.ndim == 1 ||
                (info.ndim == 2 && (info.shape[0] == 1 || info.shape[1] == 1))) {
                new (&m) Type(info.shape[0] * info.shape[1]);
                memcpy(m.data(), info.ptr, sizeof(Scalar) * m.size());
            } else {
                throw std::runtime_error("Incompatible buffer dimension!");
            }
        })
        .def("resize", [](Type &m, size_t s0) { m.resize(s0); })
        .def("conservativeResize", [](Type &m, size_t s0) { m.conservativeResize(s0); })
        .def("resizeLike", [](Type &m, const Type &m2) { m.resizeLike(m2); })
        /* Static initializers */
        .def_static("Zero", [](size_t n) -> Type { return Type::Zero(n); })
        .def_static("Ones", [](size_t n) -> Type { return Type(Type::Ones(n)); })
        .def_static("Constant", [](size_t n, Float value) -> Type { return Type::Constant(n, value); });
}

template <typename Type, typename std::enable_if<Type::SizeAtCompileTime != Eigen::Dynamic, int> = 0>
void bind_dynamic(py::class_<Type> &) { /* Nothing */ }

template <typename Type, typename std::enable_if<!Type::IsVectorAtCompileTime, int> = 0>
void bind_matrix(py::class_<Type> &) {
}

template <typename Type, typename std::enable_if<Type::IsVectorAtCompileTime, int> = 0>
void bind_matrix(py::class_<Type> &type) {
    type.def("setIdentity", [](Type &m) { m.setIdentity(); });
}

NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)

template <typename Type, typename... Args>
py::class_<Type> bind_eigen_type(py::module &m, const char *name, Args&&... args) {
    typedef typename Type::Scalar Scalar;

    /* Many Eigen functions are templated and can't easily be referenced using
       a function pointer, thus a big portion of the binding code below
       instantiates Eigen code using small anonymous wrapper functions */
    py::class_<Type> type(m, name, args...);

    type
        /* Constructors */
        .def(py::init<>())

        /* Size query functions */
        .def("size", [](const Type &m) { return m.size(); })
        .def("cols", &Type::cols)
        .def("rows", &Type::rows)

        /* Arithmetic operators (def_cast forcefully casts the result back to a
           Type to avoid type issues with Eigen's crazy expression templates) */
        .def_cast(-py::self)
        .def_cast(py::self + py::self)
        .def_cast(py::self - py::self)
        .def_cast(py::self * Scalar())
        .def_cast(py::self / Scalar())

        /* Arithmetic in-place operators */
        .def_cast(py::self += py::self)
        .def_cast(py::self -= py::self)
        .def_cast(py::self *= Scalar())
        .def_cast(py::self /= Scalar())

        /* Comparison operators */
        .def(py::self == py::self)
        .def(py::self != py::self)

        /* Buffer access for interacting with NumPy */
        .def_buffer([](Type &m) -> py::buffer_info {
            return py::buffer_info(
                m.data(),                /* Pointer to buffer */
                sizeof(Scalar),          /* Size of one scalar */
                /* Python struct-style format descriptor */
                py::format_descriptor<Scalar>::value(),
                2,                       /* Number of dimensions */
                { (size_t) m.rows(),     /* Buffer dimensions */
                  (size_t) m.cols() },
                { sizeof(Scalar),        /* Strides (in bytes) for each index */
                  sizeof(Scalar) * m.rows() }
            );
         })

        /* Initialization */
        .def("setZero", [](Type &m) { m.setZero(); })
        .def("setConstant", [](Type &m, Scalar value) { m.setConstant(value); })

        .def("__repr__", [](const Type &v) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        });


    //detail::bind_dynamic(type);
    //detail::bind_matrix(type);

    #if 0
        /* Component-wise operations */
        .def("cwiseAbs", &Type::cwiseAbs)
        .def("cwiseAbs2", &Type::cwiseAbs2)
        .def("cwiseSqrt", &Type::cwiseSqrt)
        .def("cwiseInverse", &Type::cwiseInverse)
        .def("cwiseMin", [](const Type &m1, const Type &m2) -> Type { return m1.cwiseMin(m2); })
        .def("cwiseMax", [](const Type &m1, const Type &m2) -> Type { return m1.cwiseMax(m2); })
        .def("cwiseMin", [](const Type &m1, Scalar s) -> Type { return m1.cwiseMin(s); })
        .def("cwiseMax", [](const Type &m1, Scalar s) -> Type { return m1.cwiseMax(s); })
        .def("cwiseProduct", [](const Type &m1, const Type &m2) -> Type { return m1.cwiseProduct(m2); })
        .def("cwiseQuotient", [](const Type &m1, const Type &m2) -> Type { return m1.cwiseQuotient(m2); })
    #endif

    return type;
}

MTS_PY_EXPORT(vector) {
    bind_eigen_type<Vector3f>(m, "Vector3f");
}
