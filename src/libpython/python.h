#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>
#include "docstr.h"
#include <mitsuba/core/object.h>
#include <simdfloat/static.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, mitsuba::ref<T>);

#define D(...) DOC(__VA_ARGS__)
#define DM(...) DOC(mitsuba, __VA_ARGS__)

#define MTS_PY_DECLARE(name) \
    extern void python_export_##name(py::module &)

#define MTS_PY_IMPORT_CORE(name) \
    python_export_##name(core)

#define MTS_PY_IMPORT_RENDER(name) \
    python_export_##name(render)

#define MTS_PY_EXPORT(name) \
    void python_export_##name(py::module &m)

#define MTS_PY_CLASS(Name, Base, ...) \
    py::class_<Name, ref<Name>>(m, #Name, DM(Name), py::base<Base>(), ##__VA_ARGS__)

#define MTS_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    py::class_<Name, ref<Name>, Trampoline>(m, #Name, DM(Name), py::base<Base>(), ##__VA_ARGS__)

#define MTS_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, DM(Name), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of methods
#define mdef(Class, Function, ...) \
    def(#Function, &Class::Function, DM(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of static methods
#define sdef(Class, Function, ...) \
    def_static(#Function, &Class::Function, DM(Class, Function), ##__VA_ARGS__)

using namespace mitsuba;

namespace py = pybind11;

template <typename T> class is_simd_float {
private:
    template <typename Scalar, size_t Dimension, bool ApproximateMath, typename Derived, typename SFINAE>
    static std::true_type test(const
          simd::StaticFloatBase<Scalar, Dimension, ApproximateMath, Derived, SFINAE> &);
    static std::false_type test(...);
public:
    static constexpr bool value = decltype(test(std::declval<T>()))::value;
};

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)
template<typename Type>
struct type_caster<Type, typename std::enable_if<is_simd_float<Type>::value>::type> {
    typedef typename Type::Scalar Scalar;

    bool load(handle src, bool) {
       array_t<Scalar> buffer(src, true);
       if (!buffer.check())
           return false;

        buffer_info info = buffer.request();
        if (info.ndim == 1) {
            if (info.shape[0] != Type::Dimension)
                return false;
            memcpy(value.data(), (Scalar *) info.ptr, Type::Dimension * sizeof(Scalar));
        } else if (info.ndim == 2) {

            if (!(info.shape[0] == Type::Dimension && info.shape[1] == 1) &&
                !(info.shape[1] == Type::Dimension && info.shape[0] == 1))
                return false;
            memcpy(value.data(), (Scalar *) info.ptr, Type::Dimension * sizeof(Scalar));
        } else {
            return false;
        }
        return true;
    }

    static handle cast(const Type *src, return_value_policy policy, handle parent) {
        return cast(*src, policy, parent);
    }

    static handle cast(const Type &src, return_value_policy /* policy */, handle /* parent */) {
        return array(buffer_info(
            /* Pointer to buffer */
            const_cast<Scalar *>(src.data()),
            /* Size of one scalar */
            sizeof(Scalar),
            /* Python struct-style format descriptor */
            format_descriptor<Scalar>::value,
            /* Number of dimensions */
            1,
            /* Buffer dimensions */
            { Type::Dimension },
            /* Strides (in bytes) for each index */
            { sizeof(Scalar) }
        )).release();
    }

    template <typename _T> using cast_op_type = pybind11::detail::cast_op_type<_T>;

    static PYBIND11_DESCR name() {
        return _("numpy.ndarray[dtype=") + npy_format_descriptor<Scalar>::name() +
               _(", shape=(") + _<Type::Dimension>() + _(", 1)]");
    }

    operator Type*() { return &value; }
    operator Type&() { return value; }

private:
    Type value;
};

NAMESPACE_END(detail)
NAMESPACE_END(pybind11)
