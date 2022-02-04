#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/python/python.h>

py::handle array_init;

template <typename Array, typename Base>
void bind_dr(py::module_ &m, const char *name) {
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
void dr_bind_vp_impl(py::module_ &m, const std::string &prefix) {
    std::string suffix = std::to_string(Size);
    if (std::is_floating_point_v<dr::scalar_t<Type>>)
        suffix += "f";
    else if (std::is_signed_v<dr::scalar_t<Type>>)
        suffix += "i";
    else
        suffix += "u";

    bind_dr<Vector<Type, Size>, dr::Array<Type, Size>>(
        m, (prefix + "Vector" + suffix).c_str());
    bind_dr<Point<Type, Size>, dr::Array<Type, Size>>(
        m, (prefix + "Point" + suffix).c_str());
}

template <typename Type>
void dr_bind_vp(py::module_ &m, const std::string &prefix = "") {
    dr_bind_vp_impl<Type, 0>(m, prefix);
    dr_bind_vp_impl<Type, 1>(m, prefix);
    dr_bind_vp_impl<Type, 2>(m, prefix);
    dr_bind_vp_impl<Type, 3>(m, prefix);
    dr_bind_vp_impl<Type, 4>(m, prefix);
}

MTS_PY_EXPORT(DrJit) {
    MTS_PY_IMPORT_TYPES()

    // Import the right variant of Dr.Jit
    const char *backend = "scalar";
    if constexpr (dr::is_cuda_array_v<Float>)
        backend = "cuda";
    else if constexpr (dr::is_llvm_array_v<Float>)
        backend = "llvm";

    py::module drjit         = py::module::import("drjit"),
               drjit_variant = drjit.attr(backend),
               drjit_scalar  = drjit.attr("scalar");

    if constexpr (dr::is_diff_array_v<Float>)
        drjit_variant = drjit_variant.attr("ad");

    array_init = drjit.attr("detail").attr("array_init");

    // Create basic type aliases to Dr.Jit (scalar + vectorized)
    for (const char *name : { "Float32", "Float64", "Bool", "Int", "Int32",
                              "Int64", "UInt", "UInt32", "UInt64" }) {
        m.attr(name) = drjit_variant.attr(name);
        m.attr((std::string("Scalar") + name).c_str()) =
            drjit_scalar.attr(name);
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

    dr_bind_vp<Float>(m);
    dr_bind_vp<Int32>(m);
    dr_bind_vp<UInt32>(m);
    dr_bind_vp<ScalarFloat>(m, "Scalar");
    dr_bind_vp<ScalarInt32>(m, "Scalar");
    dr_bind_vp<ScalarUInt32>(m, "Scalar");

    bind_dr<Color<Float, 0>, dr::Array<Float, 0>>(m, "Color0f");
    bind_dr<Color<Float, 1>, dr::Array<Float, 1>>(m, "Color1f");
    bind_dr<Color<Float, 3>, dr::Array<Float, 3>>(m, "Color3f");
    bind_dr<Color<ScalarFloat, 0>, dr::Array<ScalarFloat, 0>>(m, "ScalarColor0f");
    bind_dr<Color<ScalarFloat, 1>, dr::Array<ScalarFloat, 1>>(m, "ScalarColor1f");
    bind_dr<Color<ScalarFloat, 3>, dr::Array<ScalarFloat, 3>>(m, "ScalarColor3f");

    bind_dr<Color<ScalarFloat64, 0>, dr::Array<ScalarFloat64, 0>>(m, "ScalarColor0d");
    bind_dr<Color<ScalarFloat64, 1>, dr::Array<ScalarFloat64, 1>>(m, "ScalarColor1d");
    bind_dr<Color<ScalarFloat64, 3>, dr::Array<ScalarFloat64, 3>>(m, "ScalarColor3d");

    bind_dr<Normal3f, dr::Array<Float, 3>>(m, "Normal3f");
    bind_dr<ScalarNormal3f, dr::Array<ScalarFloat, 3>>(m, "ScalarNormal3f");

    using DrSpec = dr::Array<dr::value_t<UnpolarizedSpectrum>,
                             dr::array_size_v<UnpolarizedSpectrum>>;
    if constexpr (is_polarized_v<Spectrum>) {
        bind_dr<Spectrum, dr::Matrix<DrSpec, 4>>(m, "Spectrum");
        bind_dr<UnpolarizedSpectrum, DrSpec>(m, "UnpolarizedSpectrum");
    } else {
        bind_dr<Spectrum, DrSpec>(m, "Spectrum");
        m.attr("UnpolarizedSpectrum") = m.attr("Spectrum");
    }

    // Matrix type aliases
    for (int dim = 2; dim < 5; ++dim) {
        std::string mts_name = "Matrix" + std::to_string(dim) + "f",
                    dr_name  = mts_name;
        if constexpr (std::is_same_v<double, ScalarFloat>)
            dr_name += "64";

        m.attr(mts_name.c_str()) =
            drjit_variant.attr(dr_name.c_str());

        m.attr(("Scalar" + mts_name).c_str()) =
            drjit_scalar.attr(dr_name.c_str());
    }

    if constexpr (std::is_same_v<float, ScalarFloat>)
        m.attr("TensorXf") = drjit_variant.attr("TensorXf");
    else
        m.attr("TensorXf") = drjit_variant.attr("TensorXf64");

    m.attr("PCG32") = drjit_variant.attr("PCG32");
    m.attr("Loop") = drjit_variant.attr("Loop");
}

