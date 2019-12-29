#include <mitsuba/python/python.h>
#include <mitsuba/core/vector.h>

MTS_PY_DECLARE(Ray);
MTS_PY_DECLARE(DiscreteDistribution);
MTS_PY_DECLARE(ContinuousDistribution);

#define MODULE_NAME MTS_MODULE_NAME(core, MTS_VARIANT_NAME)

PYBIND11_MODULE(MODULE_NAME, m) {
    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.core";

    py::module enoki = py::module::import("enoki");

    MTS_PY_IMPORT_TYPES_DYNAMIC()

    // Create aliases of Enoki types in the Mitsuba namespace
    std::string suffix = "";
    if constexpr (is_cuda_array_v<Float> &&
                  is_diff_array_v<Float>)
        suffix = "D";
    else if constexpr (is_cuda_array_v<Float>)
        suffix = "C";
    else if constexpr (is_array_v<Float>)
        suffix = "X";

    // Basic type aliases in the Enoki module (scalar + vectorized)
    m.attr("ScalarFloat")  = enoki.attr("Float");
    m.attr("ScalarMask")   = enoki.attr("Mask");
    m.attr("ScalarInt32")  = enoki.attr("Int32");
    m.attr("ScalarInt64")  = enoki.attr("Int64");
    m.attr("ScalarUInt32") = enoki.attr("UInt32");
    m.attr("ScalarUInt64") = enoki.attr("UInt64");

    m.attr("Float")  = enoki.attr(("Float" + suffix).c_str());
    m.attr("Mask")   = enoki.attr(("Mask" + suffix).c_str());
    m.attr("Int32")  = enoki.attr(("Int32" + suffix).c_str());
    m.attr("Int64")  = enoki.attr(("Int64" + suffix).c_str());
    m.attr("UInt32") = enoki.attr(("UInt32" + suffix).c_str());
    m.attr("UInt64") = enoki.attr(("UInt64" + suffix).c_str());

    // Vector type aliases
    for (int dim = 2; dim < 4; ++dim) {
        for (int i = 0; i < 3; ++i) {
            std::string v_name = "Vector" + std::to_string(dim) + "fiu"[i],
                        p_name = "Point" + std::to_string(dim) + "fiu"[i];
            py::handle h = enoki.attr((v_name + suffix).c_str());
            m.attr(v_name.c_str()) = h;
            m.attr(p_name.c_str()) = h;

            h = enoki.attr(v_name.c_str());
            m.attr(("Scalar" + v_name).c_str()) = h;
            m.attr(("Scalar" + p_name).c_str()) = h;
        }
    }

    m.attr("Vector1f")       = enoki.attr(("Vector1f" + suffix).c_str());
    m.attr("ScalarVector1f") = enoki.attr("Vector1f");

    m.attr("Point1f")        = m.attr("Vector1f");
    m.attr("ScalarPoint1f")  = m.attr("ScalarVector1f");

    m.attr("Normal3f")       = m.attr("Vector3f");
    m.attr("ScalarNormal3f") = m.attr("ScalarVector3f");

    m.attr("Color3f")        = m.attr("Vector3f");
    m.attr("ScalarColor3f")  = m.attr("ScalarVector3f");

    m.attr("Color1f")        = m.attr("Vector1f");
    m.attr("ScalarColor1f")  = m.attr("ScalarVector1f");

    if constexpr (is_cuda_array_v<Float> && is_diff_array_v<Float>)
        m.attr("PCG32") = enoki.attr("PCG32C");
    else
        m.attr("PCG32") = enoki.attr(("PCG32" + suffix).c_str());

    /* After importing the 'enoki' module, pybind11 is aware
       of various Enoki array types (e.g. Array<Float, 3>), etc.

       Unfortunately, it is completely unaware of Mitsuba-specific array
       variants, including points, vectors, normals, etc. Creating additional
       bindings for that many flavors of vectors would be rather prohibitive,
       so a compromise is made in the Python bindings: we consider types such
       as Vector<Float, 3>, Point<Float, 3>, Array<Float, 3>, etc., to be
       identical. The following lines set up these equivalencies.
     */

    pybind11_type_alias<Array<Float, 1>,  Vector1f>();
    pybind11_type_alias<Array<Float, 1>,  Point1f>();
    pybind11_type_alias<Array<Float, 1>,  Color1f>();
    pybind11_type_alias<Array<Float, 0>,  Color<Float, 0>>();

    pybind11_type_alias<Array<Float, 2>,  Vector2f>();
    pybind11_type_alias<Array<Float, 2>,  Point2f>();
    pybind11_type_alias<Array<Int32, 2>,  Vector2i>();
    pybind11_type_alias<Array<Int32, 2>,  Point2i>();
    pybind11_type_alias<Array<UInt32, 2>, Vector2u>();
    pybind11_type_alias<Array<UInt32, 2>, Point2u>();

    pybind11_type_alias<Array<Float, 3>,  Vector3f>();
    pybind11_type_alias<Array<Float, 3>,  Color3f>();
    pybind11_type_alias<Array<Float, 3>,  Point3f>();
    pybind11_type_alias<Array<Float, 3>,  Normal3f>();
    pybind11_type_alias<Array<Int32, 3>,  Vector3i>();
    pybind11_type_alias<Array<Int32, 3>,  Point3i>();
    pybind11_type_alias<Array<UInt32, 3>, Vector3u>();
    pybind11_type_alias<Array<UInt32, 3>, Point3u>();

    pybind11_type_alias<Array<Float, 4>,  Vector4f>();
    pybind11_type_alias<Array<Float, 4>,  Point4f>();
    pybind11_type_alias<Array<Int32, 4>,  Vector4i>();
    pybind11_type_alias<Array<Int32, 4>,  Point4i>();
    pybind11_type_alias<Array<UInt32, 4>, Vector4u>();
    pybind11_type_alias<Array<UInt32, 4>, Point4u>();

    if constexpr (is_array_v<Float>) {
        pybind11_type_alias<Array<ScalarFloat, 1>,  ScalarVector1f>();
        pybind11_type_alias<Array<ScalarFloat, 1>,  ScalarPoint1f>();
        pybind11_type_alias<Array<ScalarFloat, 1>,  ScalarColor1f>();
        pybind11_type_alias<Array<ScalarFloat, 0>,  Color<ScalarFloat, 0>>();

        pybind11_type_alias<Array<ScalarFloat, 2>,  ScalarVector2f>();
        pybind11_type_alias<Array<ScalarFloat, 2>,  ScalarPoint2f>();
        pybind11_type_alias<Array<ScalarInt32, 2>,  ScalarVector2i>();
        pybind11_type_alias<Array<ScalarInt32, 2>,  ScalarPoint2i>();
        pybind11_type_alias<Array<ScalarUInt32, 2>, ScalarVector2u>();
        pybind11_type_alias<Array<ScalarUInt32, 2>, ScalarPoint2u>();

        pybind11_type_alias<Array<ScalarFloat, 3>,  ScalarVector3f>();
        pybind11_type_alias<Array<ScalarFloat, 3>,  ScalarColor3f>();
        pybind11_type_alias<Array<ScalarFloat, 3>,  ScalarPoint3f>();
        pybind11_type_alias<Array<ScalarFloat, 3>,  ScalarNormal3f>();
        pybind11_type_alias<Array<ScalarInt32, 3>,  ScalarVector3i>();
        pybind11_type_alias<Array<ScalarInt32, 3>,  ScalarPoint3i>();
        pybind11_type_alias<Array<ScalarUInt32, 3>, ScalarVector3u>();
        pybind11_type_alias<Array<ScalarUInt32, 3>, ScalarPoint3u>();

        pybind11_type_alias<Array<ScalarFloat, 4>,  ScalarVector4f>();
        pybind11_type_alias<Array<ScalarFloat, 4>,  ScalarPoint4f>();
        pybind11_type_alias<Array<ScalarInt32, 4>,  ScalarVector4i>();
        pybind11_type_alias<Array<ScalarInt32, 4>,  ScalarPoint4i>();
        pybind11_type_alias<Array<ScalarUInt32, 4>, ScalarVector4u>();
        pybind11_type_alias<Array<ScalarUInt32, 4>, ScalarPoint4u>();
    }

    if constexpr (is_spectral_v<UnpolarizedSpectrum>) {
        pybind11_type_alias<Array<Float, UnpolarizedSpectrum::Size>,
                            UnpolarizedSpectrum>();

        if constexpr (is_polarized_v<Spectrum>) {
            pybind11_type_alias<enoki::Matrix<Array<Float, UnpolarizedSpectrum::Size>, 4>,
                                Spectrum>();
        }
    }

    if constexpr (is_array_v<Float>)
        pybind11_type_alias<UInt64, replace_scalar_t<Float, const Object *>>();

    m.attr("UnpolarizedSpectrum") = get_type_handle<UnpolarizedSpectrum>();
    m.attr("Spectrum") = get_type_handle<Spectrum>();

    MTS_PY_IMPORT(Ray);
    MTS_PY_IMPORT(DiscreteDistribution);
    MTS_PY_IMPORT(ContinuousDistribution);

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." ENOKI_TOSTRING(MODULE_NAME);
}
