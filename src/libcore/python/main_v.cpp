#include <mitsuba/python/python.h>
#include <mitsuba/core/vector.h>

MTS_PY_DECLARE(Object);
MTS_PY_DECLARE(BoundingBox);
MTS_PY_DECLARE(BoundingSphere);
MTS_PY_DECLARE(Frame);
MTS_PY_DECLARE(Ray);
MTS_PY_DECLARE(DiscreteDistribution);
MTS_PY_DECLARE(ContinuousDistribution);
MTS_PY_DECLARE(math);
MTS_PY_DECLARE(qmc);
MTS_PY_DECLARE(rfilter);
MTS_PY_DECLARE(sample_tea);
MTS_PY_DECLARE(spline);
MTS_PY_DECLARE(Spectrum);
//MTS_PY_DECLARE(Transform);
//MTS_PY_DECLARE(AnimatedTransform);
MTS_PY_DECLARE(vector);
MTS_PY_DECLARE(warp);
MTS_PY_DECLARE(xml);

#define MODULE_NAME MTS_MODULE_NAME(core, MTS_VARIANT_NAME)

PYBIND11_MODULE(MODULE_NAME, m) {
    MTS_PY_IMPORT_TYPES_DYNAMIC()

    // Temporarily change the module name (for pydoc)
    m.attr("__name__") = "mitsuba.core";

    // Import the right variant of Enoki
    const char *enoki_pkg = nullptr;
    if constexpr (is_cuda_array_v<Float> &&
                  is_diff_array_v<Float>)
        enoki_pkg = "enoki.cuda_autodiff";
    else if constexpr (is_cuda_array_v<Float>)
        enoki_pkg = "enoki.cuda";
    else if constexpr (is_array_v<Float>)
        enoki_pkg = "enoki.dynamic";
    else
        enoki_pkg = "enoki.scalar";

    py::module enoki        = py::module::import(enoki_pkg);
    py::module enoki_scalar = py::module::import("enoki.scalar");

    // Basic type aliases in the Enoki module (scalar + vectorized)
    m.attr("Float32") = enoki.attr("Float32");
    m.attr("Float64") = enoki.attr("Float64");
    m.attr("Mask")    = enoki.attr("Mask");
    m.attr("Int32")   = enoki.attr("Int32");
    m.attr("Int64")   = enoki.attr("Int64");
    m.attr("UInt32")  = enoki.attr("UInt32");
    m.attr("UInt64")  = enoki.attr("UInt64");

    m.attr("ScalarFloat32") = enoki_scalar.attr("Float32");
    m.attr("ScalarFloat64") = enoki_scalar.attr("Float64");
    m.attr("ScalarMask")    = enoki_scalar.attr("Mask");
    m.attr("ScalarInt32")   = enoki_scalar.attr("Int32");
    m.attr("ScalarInt64")   = enoki_scalar.attr("Int64");
    m.attr("ScalarUInt32")  = enoki_scalar.attr("UInt32");
    m.attr("ScalarUInt64")  = enoki_scalar.attr("UInt64");

    if constexpr (std::is_same_v<float, ScalarFloat>) {
        m.attr("Float") = enoki.attr("Float32");
        m.attr("ScalarFloat") = enoki_scalar.attr("Float32");
    } else {
        m.attr("Float") = enoki.attr("Float64");
        m.attr("ScalarFloat") = enoki_scalar.attr("Float64");
    }

    // Vector type aliases
    for (int dim = 0; dim < 4; ++dim) {
        for (int i = 0; i < 3; ++i) {
            std::string name, mts_v_name, mts_p_name;

            if constexpr (std::is_same_v<float, ScalarFloat>)
                name = "Vector" + std::to_string(dim) + "fiu"[i];
            else
                name = "Vector" + std::to_string(dim) + "diu"[i];

            mts_v_name = "Vector" + std::to_string(dim) + "fiu"[i];
            mts_p_name = "Point" + std::to_string(dim) + "fiu"[i];

            py::handle h = enoki.attr(name.c_str());
            m.attr(mts_v_name.c_str()) = h;
            m.attr(mts_p_name.c_str()) = h;

            h = enoki_scalar.attr(name.c_str());
            m.attr(("Scalar" + mts_v_name).c_str()) = h;
            m.attr(("Scalar" + mts_p_name).c_str()) = h;
        }
    }

    m.attr("Normal3f")       = m.attr("Vector3f");
    m.attr("ScalarNormal3f") = m.attr("ScalarVector3f");

    m.attr("Color3f")        = m.attr("Vector3f");
    m.attr("ScalarColor3f")  = m.attr("ScalarVector3f");

    m.attr("Color1f")        = m.attr("Vector1f");
    m.attr("ScalarColor1f")  = m.attr("ScalarVector1f");

    if constexpr (is_cuda_array_v<Float> && is_diff_array_v<Float>)
        m.attr("PCG32") = py::module::import("enoki.cuda").attr("PCG32");
    else
        m.attr("PCG32") = enoki.attr("PCG32");

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

    if constexpr (is_double_v<ScalarFloat>)
        m.attr("float_dtype") = py::dtype("d");
    else
        m.attr("float_dtype") = py::dtype("f");

    // make the python module aware of enoki dynamic arrays
    if constexpr (!is_cuda_array_v<Float>)
        py::module::import("enoki.dynamic");

    MTS_PY_IMPORT(Object);
    MTS_PY_IMPORT(BoundingBox);
    MTS_PY_IMPORT(BoundingSphere);
    MTS_PY_IMPORT(Frame);
    MTS_PY_IMPORT(Ray);
    MTS_PY_IMPORT(DiscreteDistribution);
    MTS_PY_IMPORT(ContinuousDistribution);
    MTS_PY_IMPORT(math);
    // MTS_PY_IMPORT(qmc);
    MTS_PY_IMPORT(rfilter);
    MTS_PY_IMPORT(sample_tea);
    MTS_PY_IMPORT(spline);
    MTS_PY_IMPORT(Spectrum);
    //MTS_PY_IMPORT(Transform);
    //MTS_PY_IMPORT(AnimatedTransform);
    MTS_PY_IMPORT(vector);
    MTS_PY_IMPORT(warp);
    MTS_PY_IMPORT(xml);

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." ENOKI_TOSTRING(MODULE_NAME);
}
