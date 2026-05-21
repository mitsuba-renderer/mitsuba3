#include <mitsuba/render/field.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>
#include <type_traits>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyField : public Field<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Field)
    NB_TRAMPOLINE(Field, 10);

    PyField(const Properties &props) : Field(props) { }

    FieldValueType out_type() const override {
        NB_OVERRIDE(out_type);
    }

    FieldDomain domain() const override {
        NB_OVERRIDE(domain);
    }

    uint32_t out_dim() const override {
        NB_OVERRIDE(out_dim);
    }

    uint32_t args_dim() const override {
        NB_OVERRIDE(args_dim);
    }

    bool supports_scalar() const override {
        NB_OVERRIDE(supports_scalar);
    }

    bool supports_jit() const override {
        NB_OVERRIDE(supports_jit);
    }

    bool supports_surface_queries() const override {
        NB_OVERRIDE(supports_surface_queries);
    }

    bool supports_interaction_queries() const override {
        NB_OVERRIDE(supports_interaction_queries);
    }

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }

    DR_TRAMPOLINE_TRAVERSE_CB(Field)
};

template <typename Ptr, typename Cls> void bind_field_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    cls.def("eval",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true)
        .def("eval_1",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_1(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval_1",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_1(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true)
        .def("eval_color3",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_color3(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval_color3",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_color3(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true)
        .def("eval_array2",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_array2(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval_array2",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_array2(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true)
        .def("eval_array3",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_array3(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval_array3",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_array3(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true)
        .def("eval_spec",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_spec(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval_spec",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_spec(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true)
        .def("eval_array6",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_array6(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval_array6",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_array6(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true);

    if constexpr (std::is_pointer_v<Ptr>) {
        cls.def("eval_n",
            [](Ptr field, const SurfaceInteraction3f &si,
               uint32_t count, Mask active) {
                std::vector<Float> result(count);
                field->eval_n(si, result.data(), count, FieldArgs<Float>{}, active);
                return result;
            },
            "si"_a, "count"_a, "active"_a = true)
        .def("eval_n",
            [](Ptr field, const Interaction3f &it,
               uint32_t count, Mask active) {
                std::vector<Float> result(count);
                field->eval_n(it, result.data(), count, FieldArgs<Float>{}, active);
                return result;
            },
            "it"_a, "count"_a, "active"_a = true);
    }
}

MI_PY_EXPORT(Field) {
    MI_PY_IMPORT_TYPES(Field, FieldPtr)
    using PyField = PyField<Float, Spectrum>;
    using Properties = mitsuba::Properties;

    auto field = nb::class_<Field, Object, PyField>(
        m, "Field", "Base class for structured storage/query fields")
        .def(nb::init<const Properties &>(), "props"_a)
        .def("out_type", &Field::out_type)
        .def("domain", &Field::domain)
        .def("out_dim", &Field::out_dim)
        .def("args_dim", &Field::args_dim)
        .def("supports_scalar", &Field::supports_scalar)
        .def("supports_jit", &Field::supports_jit)
        .def("supports_surface_queries", &Field::supports_surface_queries)
        .def("supports_interaction_queries", &Field::supports_interaction_queries)
        .def("__repr__", &Field::to_string);

    bind_field_generic<Field *>(field);

    if constexpr (dr::is_array_v<FieldPtr>) {
        dr::ArrayBinding b;
        auto field_ptr = dr::bind_array_t<FieldPtr>(b, m, "FieldPtr");
        bind_field_generic<FieldPtr>(field_ptr);
    }

    drjit::bind_traverse(field);
}
