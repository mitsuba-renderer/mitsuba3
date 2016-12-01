#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>
#include <tbb/tbb.h>
#include "docstr.h"
#include <mitsuba/core/object.h>
#include <simdarray/array.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, mitsuba::ref<T>);

#define D(...) DOC(__VA_ARGS__)
#define DM(...) DOC(mitsuba, __VA_ARGS__)

#define MTS_PY_DECLARE(name) \
    extern void python_export_##name(py::module &)

#define MTS_PY_IMPORT(name) \
    python_export_##name(m)

#define MTS_PY_EXPORT(name) \
    void python_export_##name(py::module &m)

#define MTS_PY_CLASS(Name, Base, ...) \
    py::class_<Name, Base, ref<Name>>(m, #Name, DM(Name), ##__VA_ARGS__)

#define MTS_PY_TRAMPOLINE_CLASS(Trampoline, Name, Base, ...) \
    py::class_<Name, Base, ref<Name>, Trampoline>(m, #Name, DM(Name), ##__VA_ARGS__)

#define MTS_PY_STRUCT(Name, ...) \
    py::class_<Name>(m, #Name, DM(Name), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of methods
#define mdef(Class, Function, ...) \
    def(#Function, &Class::Function, DM(Class, Function), ##__VA_ARGS__)

/// Shorthand notation for defining most kinds of static methods
#define sdef(Class, Function, ...) \
    def_static(#Function, &Class::Function, DM(Class, Function), ##__VA_ARGS__)

#define MTS_PY_IMPORT_MODULE(Name, ModuleName) \
    auto Name = py::module::import(ModuleName); (void) m;

using namespace mitsuba;

namespace py = pybind11;

template <typename T> class is_simdarray {
private:
    template <typename T2>
    static std::true_type test(const simd::DerivedType<T2> &);
    static std::false_type test(...);
public:
    static constexpr bool value = decltype(is_simdarray::test(std::declval<const T &>()))::value;
};

extern py::dtype dtypeForStruct(const Struct *s);

NAMESPACE_BEGIN(pybind11)
NAMESPACE_BEGIN(detail)
template<typename Type>
struct type_caster<Type, typename std::enable_if<is_simdarray<Type>::value>::type> {
    typedef typename Type::Scalar Scalar;

    bool load(handle src, bool) {
       auto buffer = array_t<Scalar>::ensure(src);
       if (!buffer)
           return false;

        buffer_info info = buffer.request();
        if (info.ndim == 1) {
            if (info.shape[0] != Type::Size)
                return false;
            memcpy(value.data(), (Scalar *) info.ptr, Type::Size * sizeof(Scalar));
        } else if (info.ndim == 2) {
            if (!(info.shape[0] == Type::Size && info.shape[1] == 1) &&
                !(info.shape[1] == Type::Size && info.shape[0] == 1))
                return false;
            memcpy(value.data(), (Scalar *) info.ptr, Type::Size * sizeof(Scalar));
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
            { Type::Size },
            /* Strides (in bytes) for each index */
            { sizeof(Scalar) }
        )).release();
    }

    template <typename _T> using cast_op_type = pybind11::detail::cast_op_type<_T>;

    static PYBIND11_DESCR name() {
        return py::detail::type_descr(_("numpy.ndarray[dtype=") + npy_format_descriptor<Scalar>::name() +
               _(", shape=(") + _<Type::Size>() + _(", 1)]"));
    }

    operator Type*() { return &value; }
    operator Type&() { return value; }

private:
    Type value;
};

NAMESPACE_END(detail)
NAMESPACE_END(pybind11)

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)

template <typename Return, typename... Args, typename Func>
py::array vectorizeImpl(Func&& f, const py::array_t<typename Args::Scalar, py::array::c_style>&... args) {
    size_t nElements[] = { (args.ndim() == 2 ? args.shape(0) :
            (args.ndim() == 0 || args.ndim() > 2) ? 0 : 1) ... };

    bool compat[] = { (args.ndim() > 0 ? (args.shape(1) == Args::Size) : false)... };

    size_t count = sizeof...(Args) > 0 ? nElements[0] : 0;
    for (size_t i = 0; i<sizeof...(Args); ++i) {
        if (count != nElements[i] || !compat[i])
            throw std::runtime_error("Incompatible input dimension!");
    }
    using ReturnScalar = typename Return::Scalar;

    py::array_t<ReturnScalar> out(
        { count, Return::Size },
        { sizeof(ReturnScalar) * Return::Size, sizeof(ReturnScalar) });

    tbb::parallel_for(
        tbb::blocked_range<size_t>(0u, count),
        [&](const tbb::blocked_range<size_t> &range) {
            for (size_t i = range.begin(); i < range.end(); ++i) {
                simd::storeUnaligned(
                    (ReturnScalar *) out.mutable_data(i, 0),
                    Return(f(Args::LoadUnaligned(args.ndim() == 2 ? args.data(i, 0) : args.data())...))
                );
            }
        }
    );

    return out.squeeze();
}

template <typename T>
using VecType = typename std::conditional<std::is_arithmetic<T>::value,
                                          TVector<T, 1>, T>::type;

template <typename Func, typename Return, typename... Args>
auto vectorize(const Func &f, Return (*)(Args...)) {
    return [f](const py::array_t<
        typename VecType<py::detail::intrinsic_t<Args>>::Scalar, py::array::c_style> &... in) -> py::object {
        return vectorizeImpl<VecType<Return>, VecType<py::detail::intrinsic_t<Args>>...>(f, in...);
    };
}

NAMESPACE_END(detail)

template <typename Return, typename... Args>
auto vectorize(Return (*f)(Args...)) {
    return mitsuba::detail::vectorize<Return (*)(Args...), Return, Args...>(f, f);
}

template <typename Func>
auto vectorize(Func &&f) -> decltype(mitsuba::detail::vectorize(
    std::forward<Func>(f),
    (typename py::detail::remove_class<decltype(
         &std::remove_reference<Func>::type::operator())>::type *) nullptr)) {
    return mitsuba::detail::vectorize(
        std::forward<Func>(f),
        (typename py::detail::remove_class<decltype(
             &std::remove_reference<Func>::type::operator())>::type *) nullptr);
}

NAMESPACE_END(mitsuba)
