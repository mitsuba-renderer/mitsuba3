// Include nanobind first so its ``ref<T>`` caster is visible to Dr.Jit.
#include <nanobind/nanobind.h>
#include <mitsuba/render/field.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>
#include <type_traits>

/// Trampoline for field implementations written in Python
MI_VARIANT class PyField : public Field<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Field)
    using BaseField = mitsuba::Field<Float, Spectrum>;
    using Args = FieldArgs<Float>;

    NB_TRAMPOLINE(Field, 30);

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

    typename BaseField::FloatStorage
    eval(const SurfaceInteraction3f &si, Args args, Mask active) const override {
        using Ret = typename BaseField::FloatStorage;
        nb::detail::ticket ticket(nb_trampoline, "eval", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                si, field_args_to_python(args), active));
        return BaseField::eval(si, args, active);
    }

    typename BaseField::FloatStorage
    eval(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::FloatStorage;
        nb::detail::ticket ticket(nb_trampoline, "eval", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                it, field_args_to_python(args), active));
        return BaseField::eval(it, args, active);
    }

    Float eval_1(const SurfaceInteraction3f &si, Args args,
                 Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_1", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(nb_trampoline.base().attr(ticket.key)(
                si, field_args_to_python(args), active));
        return BaseField::eval_1(si, args, active);
    }

    Float eval_1(const Interaction3f &it, Args args,
                 Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_1", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(nb_trampoline.base().attr(ticket.key)(
                it, field_args_to_python(args), active));
        return BaseField::eval_1(it, args, active);
    }

    Color3f eval_color3(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_color3", false);
        if (ticket.key.is_valid())
            return nb::cast<Color3f>(nb_trampoline.base().attr(ticket.key)(
                si, field_args_to_python(args), active));
        return BaseField::eval_color3(si, args, active);
    }

    Color3f eval_color3(const Interaction3f &it, Args args,
                        Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_color3", false);
        if (ticket.key.is_valid())
            return nb::cast<Color3f>(nb_trampoline.base().attr(ticket.key)(
                it, field_args_to_python(args), active));
        return BaseField::eval_color3(it, args, active);
    }

    typename BaseField::Array2f
    eval_array2(const SurfaceInteraction3f &si, Args args,
                Mask active) const override {
        using Ret = typename BaseField::Array2f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array2", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                si, field_args_to_python(args), active));
        return BaseField::eval_array2(si, args, active);
    }

    typename BaseField::Array2f
    eval_array2(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::Array2f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array2", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                it, field_args_to_python(args), active));
        return BaseField::eval_array2(it, args, active);
    }

    typename BaseField::Array3f
    eval_array3(const SurfaceInteraction3f &si, Args args,
                Mask active) const override {
        using Ret = typename BaseField::Array3f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array3", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                si, field_args_to_python(args), active));
        return BaseField::eval_array3(si, args, active);
    }

    typename BaseField::Array3f
    eval_array3(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::Array3f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array3", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                it, field_args_to_python(args), active));
        return BaseField::eval_array3(it, args, active);
    }

    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si, Args args,
                                  Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_spec", false);
        if (ticket.key.is_valid())
            return nb::cast<UnpolarizedSpectrum>(
                nb_trampoline.base().attr(ticket.key)(
                    si, field_args_to_python(args), active));
        return BaseField::eval_spec(si, args, active);
    }

    UnpolarizedSpectrum eval_spec(const Interaction3f &it, Args args,
                                  Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_spec", false);
        if (ticket.key.is_valid())
            return nb::cast<UnpolarizedSpectrum>(
                nb_trampoline.base().attr(ticket.key)(
                    it, field_args_to_python(args), active));
        return BaseField::eval_spec(it, args, active);
    }

    typename BaseField::Array6f
    eval_array6(const SurfaceInteraction3f &si, Args args,
                Mask active) const override {
        using Ret = typename BaseField::Array6f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array6", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                si, field_args_to_python(args), active));
        return BaseField::eval_array6(si, args, active);
    }

    typename BaseField::Array6f
    eval_array6(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::Array6f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array6", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(nb_trampoline.base().attr(ticket.key)(
                it, field_args_to_python(args), active));
        return BaseField::eval_array6(it, args, active);
    }

    void eval_n(const SurfaceInteraction3f &si, Float *out, uint32_t count,
                Args args, Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_n", false);
        if (ticket.key.is_valid()) {
            nb::object result = nb_trampoline.base().attr(ticket.key)(
                si, count, field_args_to_python(args), active);
            copy_eval_n_result(result, out, count);
            return;
        }
        BaseField::eval_n(si, out, count, args, active);
    }

    void eval_n(const Interaction3f &it, Float *out, uint32_t count,
                Args args, Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_n", false);
        if (ticket.key.is_valid()) {
            nb::object result = nb_trampoline.base().attr(ticket.key)(
                it, count, field_args_to_python(args), active);
            copy_eval_n_result(result, out, count);
            return;
        }
        BaseField::eval_n(it, out, count, args, active);
    }

    DR_TRAMPOLINE_TRAVERSE_CB(Field)

private:
    static void copy_eval_n_result(nb::object result_o, Float *out,
                                   uint32_t count) {
        using FloatStorage = typename BaseField::FloatStorage;
        try {
            FloatStorage result = nb::cast<FloatStorage>(result_o);
            if (result.size() != count)
                Throw("Field::eval_n(): Python override returned %zu "
                      "channel(s), expected %u.", result.size(), count);
            for (uint32_t i = 0; i < count; ++i)
                out[i] = result.entry(i);
            return;
        } catch (const nb::cast_error &) { }

        std::vector<Float> result = nb::cast<std::vector<Float>>(result_o);
        if (result.size() != count)
            Throw("Field::eval_n(): Python override returned %zu channel(s), "
                  "expected %u.", result.size(), count);
        for (uint32_t i = 0; i < count; ++i)
            out[i] = result[i];
    }

    static nb::list field_args_to_python(Args args) {
        nb::list result;
        for (uint32_t i = 0; i < args.size; ++i)
            result.append(args.data[i]);
        return result;
    }
};

template <typename Float>
std::vector<Float> field_args_from_python(nb::handle args, uint32_t expected) {
    using FloatStorage = dr::DynamicArray<Float>;

    /* Convert Python argument containers into the temporary contiguous storage
       required by FieldArgs. C++ no-argument callers bypass this path. */
    std::vector<Float> result;
    if (args.is_none()) {
        if (expected != 0)
            Throw("Field argument args_dim mismatch (expected %u, got 0).",
                  expected);
        return result;
    }

    try {
        result = nb::cast<std::vector<Float>>(args);
    } catch (const nb::cast_error &) {
        try {
            FloatStorage storage = nb::cast<FloatStorage>(args);
            result.resize(storage.size());
            for (size_t i = 0; i < storage.size(); ++i)
                result[i] = storage.entry(i);
        } catch (const nb::cast_error &) {
            Throw("Field arguments must be a list, tuple, or ArrayXf value.");
        }
    }

    if (result.size() != expected)
        Throw("Field argument args_dim mismatch (expected %u, got %zu).",
              expected, result.size());

    return result;
}

template <typename Float, typename Array6f>
dr::DynamicArray<Float> array6_to_dynamic(Array6f &&input) {
    /* Python exposes eval_array6() as an ArrayXf-like value so Dr.Jit helpers
       such as dr.isfinite() and dr.allclose() work uniformly. */
    dr::DynamicArray<Float> result = dr::empty<dr::DynamicArray<Float>>(6);
    for (size_t i = 0; i < 6; ++i)
        result.entry(i) = std::move(input.entry(i));
    return result;
}

template <typename Ptr, typename Cls> void bind_field_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    cls.def("eval",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                if constexpr (dr::is_array_v<Ptr>) {
                    /* Dr.Jit virtual calls require a uniform return arity.
                       Single-channel FieldPtr::eval() can therefore use the
                       fixed eval_1() call and wrap the result explicitly. */
                    auto dim = field->out_dim();
                    if (dr::all(dim == 1u)) {
                        using FloatStorage = dr::DynamicArray<Float>;
                        FloatStorage result = dr::empty<FloatStorage>(1);
                        result.entry(0) =
                            field->eval_1(si, FieldArgs<Float>{}, active);
                        return result;
                    }
                }
                return field->eval(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true)
        .def("eval",
            [](Ptr field, const Interaction3f &it, Mask active) {
                if constexpr (dr::is_array_v<Ptr>) {
                    /* See the SurfaceInteraction overload above. */
                    auto dim = field->out_dim();
                    if (dr::all(dim == 1u)) {
                        using FloatStorage = dr::DynamicArray<Float>;
                        FloatStorage result = dr::empty<FloatStorage>(1);
                        result.entry(0) =
                            field->eval_1(it, FieldArgs<Float>{}, active);
                        return result;
                    }
                }
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
                auto result = field->eval_array6(si, FieldArgs<Float>{}, active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "si"_a, "active"_a = true)
        .def("eval_array6",
            [](Ptr field, const Interaction3f &it, Mask active) {
                auto result = field->eval_array6(it, FieldArgs<Float>{}, active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "it"_a, "active"_a = true);

    if constexpr (std::is_pointer_v<Ptr>) {
        cls.def("eval",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_1",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_1(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_1",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_1(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_color3",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_color3(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_color3",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_color3(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_array2",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array2(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_array2",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array2(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_array3",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array3(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_array3",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array3(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_spec",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_spec(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_spec",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_spec(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_array6",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                auto result = field->eval_array6(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_array6",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                auto result = field->eval_array6(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true);

        cls.def("eval_n",
            [](Ptr field, const SurfaceInteraction3f &si,
               uint32_t count, nb::object args, Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                std::vector<Float> result(count);
                field->eval_n(si, result.data(), count,
                              FieldArgs<Float>(
                                  storage.data(), (uint32_t) storage.size()),
                              active);
                return result;
            },
            "si"_a, "count"_a, "args"_a = nb::none(), "active"_a = true)
        .def("eval_n",
            [](Ptr field, const Interaction3f &it,
               uint32_t count, nb::object args, Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                std::vector<Float> result(count);
                field->eval_n(it, result.data(), count,
                              FieldArgs<Float>(
                                  storage.data(), (uint32_t) storage.size()),
                              active);
                return result;
            },
            "it"_a, "count"_a, "args"_a = nb::none(), "active"_a = true);
    }
}

MI_PY_EXPORT(Field) {
    MI_PY_IMPORT_TYPES(Field, FieldPtr)
    using PyField = PyField<Float, Spectrum>;
    using Properties = mitsuba::Properties;

    auto field = nb::class_<Field, Object, PyField>(
        m, "Field", "Base class of all field implementations")
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
