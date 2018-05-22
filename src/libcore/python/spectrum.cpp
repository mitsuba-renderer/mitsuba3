#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Spectrum) {
    m.def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<const Spectrumf &>),
          "wavelengths"_a, "active"_a = true, D(cie1931_xyz))
     .def("cie1931_xyz", vectorize_wrapper(&cie1931_xyz<SpectrumfP>),
          "wavelengths"_a, "active"_a = true, D(cie1931_xyz))

     .def("cie1931_y", vectorize_wrapper(&cie1931_y<const Spectrumf &>),
          "wavelengths"_a, "active"_a = true, D(cie1931_y))
     .def("cie1931_y", vectorize_wrapper(&cie1931_y<SpectrumfP>),
          "wavelengths"_a, "active"_a = true, D(cie1931_y))

     //.def("luminance", vectorize_wrapper(&luminance<const Spectrumf &>),
          //"spectrum"_a, "wavelengths"_a, "active"_a = true, D(luminance))
     //.def("luminance", vectorize_wrapper(&luminance<SpectrumfP>),
          //"spectrum"_a, "wavelengths"_a, "active"_a = true, D(luminance))

     .def("sample_rgb_spectrum", vectorize_wrapper(&sample_rgb_spectrum<Float>),
          "sample"_a, D(sample_rgb_spectrum))
     .def("sample_rgb_spectrum", vectorize_wrapper(&sample_rgb_spectrum<FloatP>),
          "sample"_a, D(sample_rgb_spectrum))
     ;

    m.attr("MTS_WAVELENGTH_SAMPLES") = MTS_WAVELENGTH_SAMPLES;
    m.attr("MTS_WAVELENGTH_MIN") = MTS_WAVELENGTH_MIN;
    m.attr("MTS_WAVELENGTH_MAX") = MTS_WAVELENGTH_MAX;
}
