#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/python/python.h>

py::handle array_init;

template <typename Array, typename Base>
void bind_ek(py::module_ &m, const char *name) {
    py::handle h = get_type_handle<Array>();
    if (h.ptr()) {
        m.attr(name) = h;
        return;
    }

    py::class_<Array> cls(m, name, py::type::handle_of<Base>());
    cls.def(
        "__init__",
        [](py::detail::value_and_holder &v_h, py::args args) {
            v_h.value_ptr() = new Array();
            array_init(py::handle((PyObject *) v_h.inst), args);
        },
        py::detail::is_new_style_constructor());

    auto tinfo_array = py::detail::get_type_info(typeid(Array));
    auto tinfo_base  = py::detail::get_type_info(typeid(Base));
    tinfo_array->implicit_conversions = tinfo_base->implicit_conversions;
}

template <typename Type, size_t Size>
void ek_bind_vp_impl(py::module_ &m, const std::string &prefix) {
    std::string suffix = std::to_string(Size);
    if (std::is_floating_point_v<ek::scalar_t<Type>>)
        suffix += "f";
    else if (std::is_signed_v<ek::scalar_t<Type>>)
        suffix += "i";
    else
        suffix += "u";

    bind_ek<Vector<Type, Size>, ek::Array<Type, Size>>(
        m, (prefix + "Vector" + suffix).c_str());
    bind_ek<Point<Type, Size>, ek::Array<Type, Size>>(
        m, (prefix + "Point" + suffix).c_str());
}

template <typename Type>
void ek_bind_vp(py::module_ &m, const std::string &prefix = "") {
    ek_bind_vp_impl<Type, 0>(m, prefix);
    ek_bind_vp_impl<Type, 1>(m, prefix);
    ek_bind_vp_impl<Type, 2>(m, prefix);
    ek_bind_vp_impl<Type, 3>(m, prefix);
    ek_bind_vp_impl<Type, 4>(m, prefix);
}

MTS_PY_EXPORT(Enoki) {
    MTS_PY_IMPORT_TYPES()

    // Import the right variant of Enoki
    const char *backend = "scalar";
    if constexpr (ek::is_cuda_array_v<Float>)
        backend = "cuda";
    else if constexpr (ek::is_llvm_array_v<Float>)
        backend = "llvm";

    py::module enoki         = py::module::import("enoki"),
               enoki_variant = enoki.attr(backend),
               enoki_scalar  = enoki.attr("scalar");

    if constexpr (ek::is_diff_array_v<Float>)
        enoki_variant = enoki_variant.attr("ad");

    array_init = enoki.attr("detail").attr("array_init");

    // Create basic type aliases to Enoki (scalar + vectorized)
    for (const char *name : { "Float32", "Float64", "Bool", "Int", "Int32",
                              "Int64", "UInt", "UInt32", "UInt64" }) {
        m.attr(name) = enoki_variant.attr(name);
        m.attr((std::string("Scalar") + name).c_str()) =
            enoki_scalar.attr(name);
    }

    m.attr("Mask")       = m.attr("Bool");
    m.attr("ScalarMask") = m.attr("ScalarBool");

    if constexpr (std::is_same_v<float, ScalarFloat>) {
        m.attr("Float")       = m.attr("Float32");
        m.attr("ScalarFloat") = m.attr("ScalarFloat32");
    } else {
        m.attr("Float")       = m.attr("Float64");
        m.attr("ScalarFloat") = m.attr("ScalarFloat64");
    }

    ek_bind_vp<Float>(m);
    ek_bind_vp<Int32>(m);
    ek_bind_vp<UInt32>(m);
    ek_bind_vp<ScalarFloat>(m, "Scalar");
    ek_bind_vp<ScalarInt32>(m, "Scalar");
    ek_bind_vp<ScalarUInt32>(m, "Scalar");

    bind_ek<Color<Float, 0>, ek::Array<Float, 0>>(m, "Color0f");
    bind_ek<Color<Float, 1>, ek::Array<Float, 1>>(m, "Color1f");
    bind_ek<Color<Float, 3>, ek::Array<Float, 3>>(m, "Color3f");
    bind_ek<Color<ScalarFloat, 0>, ek::Array<ScalarFloat, 0>>(m, "ScalarColor0f");
    bind_ek<Color<ScalarFloat, 1>, ek::Array<ScalarFloat, 1>>(m, "ScalarColor1f");
    bind_ek<Color<ScalarFloat, 3>, ek::Array<ScalarFloat, 3>>(m, "ScalarColor3f");

    bind_ek<Normal3f, ek::Array<Float, 3>>(m, "Normal3f");
    bind_ek<ScalarNormal3f, ek::Array<ScalarFloat, 3>>(m, "ScalarNormal3f");

    using EkSpec = ek::Array<ek::value_t<UnpolarizedSpectrum>,
                             ek::array_size_v<UnpolarizedSpectrum>>;
    if constexpr (is_polarized_v<Spectrum>) {
        bind_ek<Spectrum, ek::Matrix<EkSpec, 4>>(m, "Spectrum");
        bind_ek<UnpolarizedSpectrum, EkSpec>(m, "UnpolarizedSpectrum");
    } else {
        bind_ek<Spectrum, EkSpec>(m, "Spectrum");
        m.attr("UnpolarizedSpectrum") = m.attr("Spectrum");
    }

    // Matrix type aliases
    for (int dim = 2; dim < 5; ++dim) {
        std::string mts_name = "Matrix" + std::to_string(dim) + "f",
                    ek_name  = mts_name;
        if constexpr (std::is_same_v<double, ScalarFloat>)
            ek_name += "64";

        m.attr(mts_name.c_str()) =
            enoki_variant.attr(ek_name.c_str());

        m.attr(("Scalar" + mts_name).c_str()) =
            enoki_scalar.attr(ek_name.c_str());
    }


    // Matrix type aliases
    m.attr("PCG32") = enoki_variant.attr("PCG32");
}

