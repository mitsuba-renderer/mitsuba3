#include <mitsuba/core/properties.h>
#include <mitsuba/render/postprocess.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyPostProcess : public PostProcess<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(PostProcess)
    NB_TRAMPOLINE(PostProcess);

    PyPostProcess(const Properties &props) : PostProcess(props) { }

    TensorXf eval(const TensorXf &image,
                  const std::vector<std::string> &channels) const override {
        NB_OVERRIDE_PURE(eval, image, channels);
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }
};

MI_PY_EXPORT(PostProcess) {
    MI_PY_IMPORT_TYPES(PostProcess)
    using PyPostProcess = PyPostProcess<Float, Spectrum>;
    using Properties = mitsuba::Properties;

    auto postprocess = MI_PY_TRAMPOLINE_CLASS(PyPostProcess, PostProcess, Object)
        .def(nb::init<const Properties &>(), "props"_a)
        .def_method(PostProcess, eval, "image"_a, "channels"_a)
        .def_method(PostProcess, to_string);

    drjit::bind_traverse(postprocess);
}
