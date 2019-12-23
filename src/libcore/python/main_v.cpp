#include <mitsuba/python/python.h>

#include <mitsuba/core/ray.h>
#include <mitsuba/render/fwd.h>

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

    m.attr("Float")    = enoki.attr(("Float" + suffix).c_str());
    m.attr("Mask")     = enoki.attr(("Mask" + suffix).c_str());
    m.attr("Int32")    = enoki.attr(("Int32" + suffix).c_str());
    m.attr("Int64")    = enoki.attr(("Int64" + suffix).c_str());
    m.attr("UInt32")   = enoki.attr(("UInt32" + suffix).c_str());
    m.attr("UInt64")   = enoki.attr(("UInt64" + suffix).c_str());

    for (int dim = 2; dim < 4; ++dim) {
        for (int i = 0; i < 3; ++i) {
            std::string v_name = "Vector" + std::to_string(dim) + "fiu"[i],
                        p_name = "Point" + std::to_string(dim) + "fiu"[i];
            py::handle h = enoki.attr((v_name + suffix).c_str());
            m.attr(v_name.c_str()) = h;
            m.attr(p_name.c_str()) = h;
        }
    }

    m.attr("Normal3f") = m.attr("Vector3f");
    m.attr("Color3f") = m.attr("Vector3f");
    m.attr("Vector1f") = enoki.attr(("Vector1f" + suffix).c_str());

    pybind11_type_alias<Array<Float, 1>, Vector1f>();
    pybind11_type_alias<Array<Float, 1>, Point1f>();
    pybind11_type_alias<Array<Float, 1>, Color1f>();

    pybind11_type_alias<Array<Float, 2>, Vector2f>();
    pybind11_type_alias<Array<Float, 2>, Point2f>();
    pybind11_type_alias<Array<Int32, 2>, Vector2i>();
    pybind11_type_alias<Array<Int32, 2>, Point2i>();
    pybind11_type_alias<Array<UInt32, 2>, Vector2u>();
    pybind11_type_alias<Array<UInt32, 2>, Point2u>();

    pybind11_type_alias<Array<Float, 3>, Vector3f>();
    pybind11_type_alias<Array<Float, 3>, Color3f>();
    pybind11_type_alias<Array<Float, 3>, Point3f>();
    pybind11_type_alias<Array<Float, 3>, Normal3f>();
    pybind11_type_alias<Array<Int32, 3>, Vector3i>();
    pybind11_type_alias<Array<Int32, 3>, Point3i>();
    pybind11_type_alias<Array<UInt32, 3>, Vector3u>();
    pybind11_type_alias<Array<UInt32, 3>, Point3u>();

    pybind11_type_alias<Array<Float, 4>, Vector4f>();
    pybind11_type_alias<Array<Float, 4>, Point4f>();
    pybind11_type_alias<Array<Int32, 4>, Vector4i>();
    pybind11_type_alias<Array<Int32, 4>, Point4i>();
    pybind11_type_alias<Array<UInt32, 4>, Vector4u>();
    pybind11_type_alias<Array<UInt32, 4>, Point4u>();

    if constexpr (is_spectral_v<UnpolarizedSpectrum>) {
        pybind11_type_alias<Array<Float, UnpolarizedSpectrum::Size>,
                            UnpolarizedSpectrum>();

        if constexpr (is_polarized_v<Spectrum>) {
            pybind11_type_alias<enoki::Matrix<Array<Float, UnpolarizedSpectrum::Size>, 4>,
                                Spectrum>();
        }
    }

    m.attr("UnpolarizedSpectrum") = get_type_handle<UnpolarizedSpectrum>();
    m.attr("Spectrum") = get_type_handle<Spectrum>();

    py::class_<Ray3f>(m, "Ray3f", D(Ray))
        .def(py::init<>(), "Create an unitialized ray")
        .def(py::init<const Ray3f &>(), "Copy constructor", "other"_a)
        .def(py::init<Point3f, Vector3f, Float, const Wavelength &>(),
            D(Ray, Ray, 5), "o"_a, "d"_a, "time"_a, "wavelengths"_a)
        .def(py::init<Point3f, Vector3f, Float, Float, Float, const Wavelength &>(),
            D(Ray, Ray, 6), "o"_a, "d"_a, "mint"_a, "maxt"_a, "time"_a, "wavelengths"_a)
        .def(py::init<const Ray3f &, Float, Float>(),
            D(Ray, Ray, 7), "other"_a, "mint"_a, "maxt"_a)
        .def("update", &Ray3f::update, D(Ray, update))
        .def("__call__", &Ray3f::operator(), D(Ray, operator, call))
        .def_field(Ray3f, o,           D(Ray, o))
        .def_field(Ray3f, d,           D(Ray, d))
        .def_field(Ray3f, d_rcp,       D(Ray, d_rcp))
        .def_field(Ray3f, mint,        D(Ray, mint))
        .def_field(Ray3f, maxt,        D(Ray, maxt))
        .def_field(Ray3f, time,        D(Ray, time))
        .def_field(Ray3f, wavelengths, D(Ray, wavelengths))
        .def_repr(Ray3f);

    //m.def("test", vectorize([](Float x) { return x * 2; }));

    // Change module name back to correct value
    m.attr("__name__") = "mitsuba." ENOKI_TOSTRING(MODULE_NAME);
}
