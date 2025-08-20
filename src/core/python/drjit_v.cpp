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
    for (const char *name : { "Float16", "Float32", "Float64", "Bool",
                              "Int8", "Int", "Int32", "Int64",
                              "UInt8", "UInt", "UInt32", "UInt64" }) {
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
        bind_dr<dr::value_t<Spectrum>>(m, "SpectrumEntry");
        bind_dr<Spectrum>(m, "Spectrum");
    } else {
        bind_dr<Spectrum>(m, "Spectrum");
        m.attr("UnpolarizedSpectrum") = m.attr("Spectrum");
    }

    // Define suffix mappings based on ScalarFloat precision
    struct SuffixMapping {
        const char *mitsuba_suffix;  // Suffix used in Mitsuba
        const char *drjit_suffix;    // Suffix used in Dr.Jit
    };

    // Floating point suffix mappings depend on ScalarFloat type
    constexpr bool is_double_precision = std::is_same_v<double, ScalarFloat>;
    constexpr SuffixMapping float_mappings[] = {
        { "f16", "f16" },
        { "f",   is_double_precision ? "f64" : "f" },
        { "f32", "f" },
        { "f16", "f16" },
        { "f64", "f64" },
        { "d",   "f64" }
    };

    // Integer and other type mappings are fixed
    constexpr SuffixMapping int_mappings[] = {
        { "i",   "i" },
        { "i8",  "i8" },
        { "i32", "i" },
        { "i64", "i64" },
        { "u8",  "u8" },
        { "u",   "u" },
        { "u32", "u" },
        { "u64", "u64" },
        { "b",   "b" }
    };

    // Generic lambda to bind aliases with automatic array size deduction
    auto bind_aliases = [&](const std::string &prefix, auto &mappings) {
        for (const auto &map : mappings) {
            std::string name = prefix + map.mitsuba_suffix;
            std::string dr_name = prefix + map.drjit_suffix;

            nb::object value = nb::getattr(drjit_variant, dr_name.c_str(), nb::handle());

            if (value.is_valid())
                m.attr(name.c_str()) = value;

            value = nb::getattr(drjit_scalar, dr_name.c_str(), nb::handle());

            if (value.is_valid())
                m.attr(("Scalar" + name).c_str()) = value;
        }
    };

    // Bind all type families
    const char *type_families[] = {
        "TensorX", "ArrayX", "Complex2", "Quaternion4"
    };

    for (const char *family : type_families) {
        bind_aliases(family, float_mappings);
        if (family[0] == 'T' || family[0] == 'A') // TensorX and ArrayX also have integer types
            bind_aliases(family, int_mappings);
    }

    // Handle dimensioned types (Matrix, Texture)
    for (int dim = 2; dim <= 4; ++dim) {
        std::string matrix = "Matrix" + std::to_string(dim);
        bind_aliases(matrix, float_mappings);
    }

    for (int dim = 1; dim <= 3; ++dim) {
        std::string texture = "Texture" + std::to_string(dim);
        bind_aliases(texture, float_mappings);
    }

    // PCG32 type alias
    m.attr("PCG32") = drjit_variant.attr("PCG32");

    // Loop type alias
    m.attr("while_loop") = drjit.attr("while_loop");
}
