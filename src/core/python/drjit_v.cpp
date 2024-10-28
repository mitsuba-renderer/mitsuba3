#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/python/python.h>
#include <drjit/python.h>

template <typename Array>
void bind_dr(nb::module_ &m, const char *name) {
    // Check if Array has already been bound (it is just a C++ alias)
    nb::handle h = nb::type<Array>();
    if (h.ptr()) {
        m.attr(name) = h;
        return;
    }

    dr::ArrayBinding b;
    dr::bind_array_t<Array>(b, m, name);
}

template <typename Type, size_t Size, bool IsDouble>
void dr_bind_vp_impl(nb::module_ &m, const std::string &prefix) {
    std::string suffix = std::to_string(Size);

    if constexpr (IsDouble)
        suffix += "d";
    else if constexpr (std::is_floating_point_v<dr::scalar_t<Type>>)
        suffix += "f";
    else if constexpr (std::is_signed_v<dr::scalar_t<Type>>)
        suffix += "i";
    else if constexpr (std::is_signed_v<dr::scalar_t<Type>>)
        suffix += "i";
    else
        suffix += "u";

    bind_dr<Vector<Type, Size>>(m, (prefix + "Vector" + suffix).c_str());
    bind_dr<Point<Type, Size>>(m, (prefix + "Point" + suffix).c_str());
}

template <typename Type, bool IsDouble = false>
void dr_bind_vp(nb::module_ &m, const std::string &prefix = "") {
    dr_bind_vp_impl<Type, 0, IsDouble>(m, prefix);
    dr_bind_vp_impl<Type, 1, IsDouble>(m, prefix);
    dr_bind_vp_impl<Type, 2, IsDouble>(m, prefix);
    dr_bind_vp_impl<Type, 3, IsDouble>(m, prefix);
    dr_bind_vp_impl<Type, 4, IsDouble>(m, prefix);
}

MI_PY_EXPORT(DrJit) {
    MI_PY_IMPORT_TYPES()

    // Import the right variant of Dr.Jit
    const char *backend = "scalar";
    if constexpr (dr::is_cuda_v<Float>)
        backend = "cuda";
    else if constexpr (dr::is_llvm_v<Float>)
        backend = "llvm";

    nb::module_ drjit         = nb::module_::import_("drjit"),
                drjit_variant = drjit.attr(backend),
                drjit_scalar  = drjit.attr("scalar");

    if constexpr (dr::is_diff_v<Float>)
        drjit_variant = drjit_variant.attr("ad");

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
    dr_bind_vp<Float64, true>(m);
    dr_bind_vp<ScalarFloat64, true>(m, "Scalar");

    bind_dr<Color<Float, 0>>(m, "Color0f");
    bind_dr<Color<Float, 1>>(m, "Color1f");
    bind_dr<Color<Float, 3>>(m, "Color3f");
    bind_dr<Color<ScalarFloat, 0>>(m, "ScalarColor0f");
    bind_dr<Color<ScalarFloat, 1>>(m, "ScalarColor1f");
    bind_dr<Color<ScalarFloat, 3>>(m, "ScalarColor3f");

    bind_dr<Color<Float64, 0>>(m, "Color0d");
    bind_dr<Color<Float64, 1>>(m, "Color1d");
    bind_dr<Color<Float64, 3>>(m, "Color3d");
    bind_dr<Color<ScalarFloat64, 0>>(m, "ScalarColor0d");
    bind_dr<Color<ScalarFloat64, 1>>(m, "ScalarColor1d");
    bind_dr<Color<ScalarFloat64, 3>>(m, "ScalarColor3d");

    bind_dr<Normal3f>(m, "Normal3f");
    bind_dr<Normal3d>(m, "Normal3d");
    bind_dr<ScalarNormal3f>(m, "ScalarNormal3f");
    bind_dr<ScalarNormal3d>(m, "ScalarNormal3d");

    if constexpr (is_polarized_v<Spectrum>) {
        bind_dr<UnpolarizedSpectrum>(m, "UnpolarizedSpectrum");
        bind_dr<dr::value_t<Spectrum>>(m, "Spectrum_vt");
        bind_dr<Spectrum>(m, "Spectrum");
    } else {
        bind_dr<Spectrum>(m, "Spectrum");
        m.attr("UnpolarizedSpectrum") = m.attr("Spectrum");
    }

    auto bind_type_aliases = [&](const std::string &name) {
        std::string dr_name  = name + "f";
        if constexpr (std::is_same_v<double, ScalarFloat>)
            dr_name += "64";
        m.attr((name + "f").c_str()) =
            drjit_variant.attr(dr_name.c_str());
        m.attr(("Scalar" + name + "f").c_str()) =
            drjit_scalar.attr(dr_name.c_str());

        if constexpr (!std::is_same_v<double, ScalarFloat>)
            dr_name += "64";

        m.attr((name + "d").c_str()) =
            drjit_variant.attr(dr_name.c_str());
        m.attr(("Scalar" + name + "d").c_str()) =
            drjit_scalar.attr(dr_name.c_str());
    };

    // Matrix type aliases
    for (int dim = 2; dim < 5; ++dim)
        bind_type_aliases("Matrix" + std::to_string(dim));

    // Complex type aliases
    bind_type_aliases("Complex2");

    // Quaternion type aliases
    bind_type_aliases("Quaternion4");

    // Tensor type aliases
    if constexpr (std::is_same_v<float, ScalarFloat>)
        m.attr("TensorXf") = drjit_variant.attr("TensorXf");
    else
        m.attr("TensorXf") = drjit_variant.attr("TensorXf64");
    m.attr("TensorXd") = drjit_variant.attr("TensorXf64");
    m.attr("TensorXi") = drjit_variant.attr("TensorXi");
    m.attr("TensorXi64") = drjit_variant.attr("TensorXi64");
    m.attr("TensorXb") = drjit_variant.attr("TensorXb");
    m.attr("TensorXu") = drjit_variant.attr("TensorXu");
    m.attr("TensorXu64") = drjit_variant.attr("TensorXu64");

    // ArrayX type aliases
    if constexpr (std::is_same_v<float, ScalarFloat>)
        m.attr("ArrayXf") = drjit_variant.attr("ArrayXf");
    else
        m.attr("ArrayXf") = drjit_variant.attr("ArrayXf64");
    m.attr("ArrayXd") = drjit_variant.attr("ArrayXf64");
    m.attr("ArrayXi") = drjit_variant.attr("ArrayXi");
    m.attr("ArrayXi64") = drjit_variant.attr("ArrayXi64");
    m.attr("ArrayXb") = drjit_variant.attr("ArrayXb");
    m.attr("ArrayXu") = drjit_variant.attr("ArrayXu");
    m.attr("ArrayXu64") = drjit_variant.attr("ArrayXu64");

    // Texture type aliases
    if constexpr (std::is_same_v<ScalarFloat, float>) {
        m.attr("Texture1f") = drjit_variant.attr("Texture1f");
        m.attr("Texture2f") = drjit_variant.attr("Texture2f");
        m.attr("Texture3f") = drjit_variant.attr("Texture3f");
    } else {
        m.attr("Texture1f") = drjit_variant.attr("Texture1f64");
        m.attr("Texture2f") = drjit_variant.attr("Texture2f64");
        m.attr("Texture3f") = drjit_variant.attr("Texture3f64");
    }
    m.attr("Texture1d") = drjit_variant.attr("Texture1f64");
    m.attr("Texture2d") = drjit_variant.attr("Texture2f64");
    m.attr("Texture3d") = drjit_variant.attr("Texture3f64");

    // PCG32 type alias
    m.attr("PCG32") = drjit_variant.attr("PCG32");

    // Loop type alias
    m.attr("while_loop") = drjit.attr("while_loop");
}
