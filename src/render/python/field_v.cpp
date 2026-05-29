// Include nanobind first so its ``ref<T>`` caster is visible to Dr.Jit.
#include <nanobind/nanobind.h>
#include <mitsuba/render/field.h>
#include <mitsuba/python/python.h>
#include <mitsuba/core/properties.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <drjit/python.h>
#include <algorithm>
#include <type_traits>

template <typename Float, typename Array6f>
Array6f field_array6_from_python(nb::handle value);

/// Trampoline for field implementations written in Python
MI_VARIANT class PyField : public Field<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Field)
    using BaseField = mitsuba::Field<Float, Spectrum>;
    using Args = FieldArgs<Float>;

    NB_TRAMPOLINE(Field, 51);

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

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask active) const override {
        return BaseField::eval(si, active);
    }

    UnpolarizedSpectrum eval(const Interaction3f &it,
                             Mask active) const override {
        return BaseField::eval(it, active);
    }

    Float eval_1(const SurfaceInteraction3f &si,
                 Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_1", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, Args{}, active));
        return BaseField::eval_1(si, active);
    }

    Float eval_1(const Interaction3f &it,
                 Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_1", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, Args{}, active));
        return BaseField::eval_1(it, active);
    }

    Color3f eval_3(const SurfaceInteraction3f &si,
                   Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_3", false);
        if (ticket.key.is_valid())
            return nb::cast<Color3f>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, Args{}, active));
        return BaseField::eval_3(si, active);
    }

    Vector3f eval_3(const Interaction3f &it,
                    Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_3", false);
        if (ticket.key.is_valid())
            return nb::cast<Vector3f>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, Args{}, active));
        return BaseField::eval_3(it, active);
    }

    typename BaseField::Array6f eval_6(const Interaction3f &it,
                                       Mask active) const override {
        using Ret = typename BaseField::Array6f;
        nb::detail::ticket ticket(nb_trampoline, "eval_6", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, Args{}, active));
        return BaseField::eval_6(it, active);
    }

    typename BaseField::Vector2f
    eval_1_grad(const SurfaceInteraction3f &si,
                Mask active) const override {
        using Ret = typename BaseField::Vector2f;
        nb::detail::ticket ticket(nb_trampoline, "eval_1_grad", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                nb_trampoline.base().attr(ticket.key)(si, active));
        return BaseField::eval_1_grad(si, active);
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &si, const Wavelength &sample,
                    Mask active) const override {
        using Ret = std::pair<Wavelength, UnpolarizedSpectrum>;
        nb::detail::ticket ticket(nb_trampoline, "sample_spectrum", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                nb_trampoline.base().attr(ticket.key)(si, sample, active));
        return BaseField::sample_spectrum(si, sample, active);
    }

    Wavelength pdf_spectrum(const SurfaceInteraction3f &si,
                            Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "pdf_spectrum", false);
        if (ticket.key.is_valid())
            return nb::cast<Wavelength>(
                nb_trampoline.base().attr(ticket.key)(si, active));
        return BaseField::pdf_spectrum(si, active);
    }

    std::pair<Point2f, Float>
    sample_position(const Point2f &sample,
                    Mask active) const override {
        using Ret = std::pair<Point2f, Float>;
        nb::detail::ticket ticket(nb_trampoline, "sample_position", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                nb_trampoline.base().attr(ticket.key)(sample, active));
        return BaseField::sample_position(sample, active);
    }

    Float pdf_position(const Point2f &p, Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "pdf_position", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(
                nb_trampoline.base().attr(ticket.key)(p, active));
        return BaseField::pdf_position(p, active);
    }

    Float mean() const override {
        nb::detail::ticket ticket(nb_trampoline, "mean", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(nb_trampoline.base().attr(ticket.key)());
        return BaseField::mean();
    }

    ScalarFloat max() const override {
        nb::detail::ticket ticket(nb_trampoline, "max", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarFloat>(
                nb_trampoline.base().attr(ticket.key)());
        return BaseField::max();
    }

    ScalarVector2i resolution_2d() const override {
        nb::detail::ticket ticket(nb_trampoline, "resolution", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarVector2i>(
                nb_trampoline.base().attr(ticket.key)());
        ticket = nb::detail::ticket(nb_trampoline, "resolution_2d", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarVector2i>(
                nb_trampoline.base().attr(ticket.key)());
        return BaseField::resolution_2d();
    }

    ScalarFloat spectral_resolution() const override {
        nb::detail::ticket ticket(nb_trampoline, "spectral_resolution", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarFloat>(
                nb_trampoline.base().attr(ticket.key)());
        return BaseField::spectral_resolution();
    }

    ScalarVector2f wavelength_range() const override {
        nb::detail::ticket ticket(nb_trampoline, "wavelength_range", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarVector2f>(
                nb_trampoline.base().attr(ticket.key)());
        return BaseField::wavelength_range();
    }

    bool is_spatially_varying() const override {
        nb::detail::ticket ticket(nb_trampoline, "is_spatially_varying", false);
        if (ticket.key.is_valid())
            return nb::cast<bool>(nb_trampoline.base().attr(ticket.key)());
        return BaseField::is_spatially_varying();
    }

    std::pair<UnpolarizedSpectrum, Vector3f>
    eval_gradient(const Interaction3f &it,
                  Mask active) const override {
        using Ret = std::pair<UnpolarizedSpectrum, Vector3f>;
        nb::detail::ticket ticket(nb_trampoline, "eval_gradient", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                nb_trampoline.base().attr(ticket.key)(it, active));
        return BaseField::eval_gradient(it, active);
    }

    void max_per_channel(ScalarFloat *out) const override {
        nb::detail::ticket ticket(nb_trampoline, "max_per_channel", false);
        if (ticket.key.is_valid()) {
            nb::object result =
                nb_trampoline.base().attr(ticket.key)();
            std::vector<ScalarFloat> values =
                nb::cast<std::vector<ScalarFloat>>(result);
            uint32_t count = this->channel_count();
            if (values.size() != count)
                Throw("Field::max_per_channel(): Python override returned %zu "
                      "value(s), expected %u.", values.size(), count);
            for (size_t i = 0; i < values.size(); ++i)
                out[i] = values[i];
            return;
        }
        BaseField::max_per_channel(out);
    }

    ScalarBoundingBox3f bbox() const override {
        nb::detail::ticket ticket(nb_trampoline, "bbox", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarBoundingBox3f>(
                nb_trampoline.base().attr(ticket.key)());
        return BaseField::bbox();
    }

    ScalarVector3i resolution_3d() const override {
        nb::detail::ticket ticket(nb_trampoline, "resolution", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarVector3i>(
                nb_trampoline.base().attr(ticket.key)());
        ticket = nb::detail::ticket(nb_trampoline, "resolution_3d", false);
        if (ticket.key.is_valid())
            return nb::cast<ScalarVector3i>(
                nb_trampoline.base().attr(ticket.key)());
        return BaseField::resolution_3d();
    }

    uint32_t channel_count() const override {
        nb::detail::ticket ticket(nb_trampoline, "channel_count", false);
        if (ticket.key.is_valid())
            return nb::cast<uint32_t>(
                nb_trampoline.base().attr(ticket.key)());
        return BaseField::channel_count();
    }

    std::string to_string() const override {
        NB_OVERRIDE(to_string);
    }

    typename BaseField::FloatStorage
    eval(const SurfaceInteraction3f &si, Args args, Mask active) const override {
        using Ret = typename BaseField::FloatStorage;
        nb::detail::ticket ticket(nb_trampoline, "eval", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, args, active));
        return BaseField::eval(si, args, active);
    }

    typename BaseField::FloatStorage
    eval(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::FloatStorage;
        nb::detail::ticket ticket(nb_trampoline, "eval", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, args, active));
        return BaseField::eval(it, args, active);
    }

    Float eval_1(const SurfaceInteraction3f &si, Args args,
                 Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_1", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, args, active));
        return BaseField::eval_1(si, args, active);
    }

    Float eval_1(const Interaction3f &it, Args args,
                 Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_1", false);
        if (ticket.key.is_valid())
            return nb::cast<Float>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, args, active));
        return BaseField::eval_1(it, args, active);
    }

    Color3f eval_color3(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_color3", false);
        if (ticket.key.is_valid())
            return nb::cast<Color3f>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, args, active));
        return BaseField::eval_color3(si, args, active);
    }

    Color3f eval_color3(const Interaction3f &it, Args args,
                        Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_color3", false);
        if (ticket.key.is_valid())
            return nb::cast<Color3f>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, args, active));
        return BaseField::eval_color3(it, args, active);
    }

    typename BaseField::Array2f
    eval_array2(const SurfaceInteraction3f &si, Args args,
                Mask active) const override {
        using Ret = typename BaseField::Array2f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array2", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, args, active));
        return BaseField::eval_array2(si, args, active);
    }

    typename BaseField::Array2f
    eval_array2(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::Array2f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array2", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, args, active));
        return BaseField::eval_array2(it, args, active);
    }

    typename BaseField::Array3f
    eval_array3(const SurfaceInteraction3f &si, Args args,
                Mask active) const override {
        using Ret = typename BaseField::Array3f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array3", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, args, active));
        return BaseField::eval_array3(si, args, active);
    }

    typename BaseField::Array3f
    eval_array3(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::Array3f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array3", false);
        if (ticket.key.is_valid())
            return nb::cast<Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, args, active));
        return BaseField::eval_array3(it, args, active);
    }

    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si, Args args,
                                  Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_spec", false);
        if (ticket.key.is_valid())
            return nb::cast<UnpolarizedSpectrum>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, args, active));
        return BaseField::eval_spec(si, args, active);
    }

    UnpolarizedSpectrum eval_spec(const Interaction3f &it, Args args,
                                  Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_spec", false);
        if (ticket.key.is_valid())
            return nb::cast<UnpolarizedSpectrum>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, args, active));
        return BaseField::eval_spec(it, args, active);
    }

    typename BaseField::Array6f
    eval_array6(const SurfaceInteraction3f &si, Args args,
                Mask active) const override {
        using Ret = typename BaseField::Array6f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array6", false);
        if (ticket.key.is_valid())
            return field_array6_from_python<Float, Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    si, args, active));
        return BaseField::eval_array6(si, args, active);
    }

    typename BaseField::Array6f
    eval_array6(const Interaction3f &it, Args args, Mask active) const override {
        using Ret = typename BaseField::Array6f;
        nb::detail::ticket ticket(nb_trampoline, "eval_array6", false);
        if (ticket.key.is_valid())
            return field_array6_from_python<Float, Ret>(
                call_field_override(nb_trampoline.base().attr(ticket.key),
                                    it, args, active));
        return BaseField::eval_array6(it, args, active);
    }

    void eval_n(const SurfaceInteraction3f &si, Float *out, uint32_t count,
                Args args, Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_n", false);
        if (ticket.key.is_valid()) {
            nb::object result = call_eval_n_override(
                nb_trampoline.base().attr(ticket.key), si, count, args, active);
            copy_eval_n_result(result, out, count);
            return;
        }
        BaseField::eval_n(si, out, count, args, active);
    }

    void eval_n(const Interaction3f &it, Float *out, uint32_t count,
                Args args, Mask active) const override {
        nb::detail::ticket ticket(nb_trampoline, "eval_n", false);
        if (ticket.key.is_valid()) {
            nb::object result = call_eval_n_override(
                nb_trampoline.base().attr(ticket.key), it, count, args, active);
            copy_eval_n_result(result, out, count);
            return;
        }
        BaseField::eval_n(it, out, count, args, active);
    }

    DR_TRAMPOLINE_TRAVERSE_CB(Field)

private:
    static bool python_callable_accepts_parameter(nb::handle callable,
                                                  std::string_view name) {
        nb::object function = nb::borrow(callable);
        if (nb::hasattr(callable, "__func__"))
            function = callable.attr("__func__");

        if (!nb::hasattr(function, "__code__"))
            return true;

        nb::object code = function.attr("__code__");
        uint32_t flags = nb::cast<uint32_t>(code.attr("co_flags"));
        constexpr uint32_t co_varargs = 0x04;
        constexpr uint32_t co_varkeywords = 0x08;
        if (flags & (co_varargs | co_varkeywords))
            return true;

        size_t arg_count =
            nb::cast<size_t>(code.attr("co_argcount")) +
            nb::cast<size_t>(code.attr("co_kwonlyargcount"));
        nb::tuple names = nb::cast<nb::tuple>(code.attr("co_varnames"));
        arg_count = std::min(arg_count, (size_t) nb::len(names));

        for (size_t i = 0; i < arg_count; ++i) {
            std::string candidate = nb::cast<std::string>(names[i]);
            if (candidate == name)
                return true;
        }

        return false;
    }

    template <typename Record>
    nb::object call_field_override(nb::object method, const Record &record,
                                   Args args, Mask active) const {
        uint32_t expected = this->args_dim();
        if (args.size != expected)
            Throw("Field argument args_dim mismatch (expected %u, got %u).",
                  expected, args.size);

        bool accepts_args = python_callable_accepts_parameter(method, "args"),
             accepts_active = python_callable_accepts_parameter(method, "active");

        if (accepts_args) {
            nb::object args_o = field_args_to_python(args);
            if (accepts_active)
                return method(record, args_o, active);
            return method(record, args_o);
        }

        if (args.size != 0)
            Throw("Field argument args_dim mismatch (expected 0, got %u).",
                  args.size);

        if (accepts_active)
            return method(record, active);
        return method(record);
    }

    template <typename Record>
    nb::object call_eval_n_override(nb::object method, const Record &record,
                                    uint32_t count, Args args,
                                    Mask active) const {
        uint32_t expected = this->args_dim();
        if (args.size != expected)
            Throw("Field argument args_dim mismatch (expected %u, got %u).",
                  expected, args.size);

        bool accepts_args = python_callable_accepts_parameter(method, "args"),
             accepts_active = python_callable_accepts_parameter(method, "active");

        if (accepts_args) {
            nb::object args_o = field_args_to_python(args);
            if (accepts_active)
                return method(record, count, args_o, active);
            return method(record, count, args_o);
        }

        if (args.size != 0)
            Throw("Field argument args_dim mismatch (expected 0, got %u).",
                  args.size);

        if (accepts_active)
            return method(record, count, active);
        return method(record, count);
    }

    static void copy_eval_n_result(nb::object result_o, Float *out,
                                   uint32_t count) {
        using FloatStorage = typename BaseField::FloatStorage;
        if (nb::isinstance<FloatStorage>(result_o)) {
            FloatStorage result = nb::cast<FloatStorage>(result_o);
            if (result.size() != count)
                Throw("Field::eval_n(): Python override returned %zu "
                      "channel(s), expected %u.", result.size(), count);
            for (uint32_t i = 0; i < count; ++i)
                out[i] = result.entry(i);
            return;
        }

        if (!nb::isinstance<nb::list>(result_o) &&
            !nb::isinstance<nb::tuple>(result_o))
            Throw("Field::eval_n(): Python override must return ArrayXf, list, "
                  "or tuple.");

        std::vector<Float> result = nb::cast<std::vector<Float>>(result_o);
        if (result.size() != count)
            Throw("Field::eval_n(): Python override returned %zu channel(s), "
                  "expected %u.", result.size(), count);
        for (uint32_t i = 0; i < count; ++i)
            out[i] = result[i];
    }

    static nb::object field_args_to_python(Args args) {
        if (args.size == 0)
            return nb::none();
        nb::list result;
        for (uint32_t i = 0; i < args.size; ++i)
            result.append(args.data[i]);
        return result;
    }
};

template <typename Float>
std::vector<Float> field_args_from_python(nb::handle args, uint32_t expected) {
    using FloatStorage = dr::DynamicArray<Float>;

    // Convert Python argument containers into the temporary contiguous storage
    // required by FieldArgs.
    std::vector<Float> result;
    if (args.is_none()) {
        if (expected != 0)
            Throw("Field argument args_dim mismatch (expected %u, got 0).",
                  expected);
        return result;
    }

    if (nb::isinstance<nb::list>(args) || nb::isinstance<nb::tuple>(args)) {
        result = nb::cast<std::vector<Float>>(args);
    } else if (nb::isinstance<FloatStorage>(args)) {
        FloatStorage storage = nb::cast<FloatStorage>(args);
        result.resize(storage.size());
        for (size_t i = 0; i < storage.size(); ++i)
            result[i] = storage.entry(i);
    } else if (PyFloat_Check(args.ptr()) || PyLong_Check(args.ptr()) ||
               nb::isinstance<Float>(args)) {
        result.push_back(nb::cast<Float>(args));
    } else {
        Throw("Field arguments must be a scalar, list, tuple, or ArrayXf "
              "value.");
    }

    if (result.size() != expected)
        Throw("Field argument args_dim mismatch (expected %u, got %zu).",
              expected, result.size());

    return result;
}

template <typename Float, typename Array6f>
Array6f field_array6_from_python(nb::handle value) {
    using FloatStorage = dr::DynamicArray<Float>;

    if (nb::isinstance<Array6f>(value))
        return nb::cast<Array6f>(value);

    Array6f result;
    if (nb::isinstance<FloatStorage>(value)) {
        FloatStorage storage = nb::cast<FloatStorage>(value);
        if (storage.size() != 6)
            Throw("Field::eval_array6(): Python override returned %zu "
                  "channel(s), expected 6.", storage.size());
        for (size_t i = 0; i < 6; ++i)
            result.entry(i) = storage.entry(i);
        return result;
    }

    if (!nb::isinstance<nb::list>(value) && !nb::isinstance<nb::tuple>(value))
        Throw("Field::eval_array6(): Python override must return Array6f, "
              "ArrayXf, list, or tuple with 6 channels.");

    std::vector<Float> storage = nb::cast<std::vector<Float>>(value);
    if (storage.size() != 6)
        Throw("Field::eval_array6(): Python override returned %zu "
              "channel(s), expected 6.", storage.size());
    for (size_t i = 0; i < 6; ++i)
        result.entry(i) = storage[i];
    return result;
}

template <typename Float, typename Array6f>
dr::DynamicArray<Float> array6_to_dynamic(Array6f &&input) {
    // Python exposes eval_array6() as an ArrayXf-like value so Dr.Jit helpers
    // such as dr.isfinite() and dr.allclose() work uniformly.
    dr::DynamicArray<Float> result = dr::empty<dr::DynamicArray<Float>>(6);
    for (size_t i = 0; i < 6; ++i)
        result.entry(i) = std::move(input.entry(i));
    return result;
}

template <typename Ptr>
void check_field_output(Ptr field, FieldValueType expected_type,
                        uint32_t expected_dim, const char *method) {
    if constexpr (std::is_pointer_v<Ptr>) {
        FieldValueType type = field->out_type();
        uint32_t dim = field->out_dim();
        if (type != expected_type || dim != expected_dim)
            Throw("%s: expected %s[%u], got %s[%u].", method,
                  mitsuba::field_value_type_name(expected_type), expected_dim,
                  mitsuba::field_value_type_name(type), dim);
    }
}

template <typename Ptr>
void check_field_count(Ptr field, uint32_t count, const char *method) {
    if constexpr (std::is_pointer_v<Ptr>) {
        uint32_t dim = field->out_dim();
        if (count != dim)
            Throw("%s: count (%u) must match out_dim (%u).",
                  method, count, dim);
    }
}

template <typename Ptr>
void check_field_args_dim_zero(Ptr field, const char *method) {
    if constexpr (std::is_pointer_v<Ptr>) {
        uint32_t dim = field->args_dim();
        if (dim != 0)
            Throw("%s: expected args_dim=0, got %u.", method, dim);
    }
}

template <typename Ptr, typename Cls> void bind_field_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    cls.def("eval",
            [](Ptr field, const SurfaceInteraction3f &si,
               Mask active) -> nb::object {
                return nb::cast(field->eval(si, active));
            },
            "si"_a, "active"_a = true, D(Field, eval, 3))
        .def("eval",
            [](Ptr field, const Interaction3f &it,
               Mask active) -> nb::object {
                return nb::cast(field->eval(it, active));
            },
            "it"_a, "active"_a = true, D(Field, eval, 4))
        .def("eval_1",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_1(si, active);
            },
            "si"_a, "active"_a = true, D(Field, eval_1, 2))
        .def("eval_1",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_1(it, active);
            },
            "it"_a, "active"_a = true, D(Field, eval_1, 4))
        .def("eval_3",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                return field->eval_3(si, active);
            },
            "si"_a, "active"_a = true, D(Field, eval_3, 3))
        .def("eval_3",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_3(it, active);
            },
            "it"_a, "active"_a = true, D(Field, eval_3, 2))
        .def("eval_6",
            [](Ptr field, const Interaction3f &it, Mask active) {
                auto result = field->eval_6(it, active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "it"_a, "active"_a = true, D(Field, eval_6))
        .def("eval_color3",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_color3()");
                check_field_output(field, FieldValueType::Color3, 3,
                                   "Field::eval_color3()");
                return field->eval_color3(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true, D(Field, eval_color3, 2))
        .def("eval_color3",
            [](Ptr field, const Interaction3f &it, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_color3()");
                check_field_output(field, FieldValueType::Color3, 3,
                                   "Field::eval_color3()");
                return field->eval_color3(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true, D(Field, eval_color3, 4))
        .def("eval_array2",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_array2()");
                check_field_output(field, FieldValueType::Array2, 2,
                                   "Field::eval_array2()");
                return field->eval_array2(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true, D(Field, eval_array2))
        .def("eval_array2",
            [](Ptr field, const Interaction3f &it, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_array2()");
                check_field_output(field, FieldValueType::Array2, 2,
                                   "Field::eval_array2()");
                return field->eval_array2(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true, D(Field, eval_array2, 2))
        .def("eval_array3",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_array3()");
                check_field_output(field, FieldValueType::Array3, 3,
                                   "Field::eval_array3()");
                return field->eval_array3(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true, D(Field, eval_array3))
        .def("eval_array3",
            [](Ptr field, const Interaction3f &it, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_array3()");
                check_field_output(field, FieldValueType::Array3, 3,
                                   "Field::eval_array3()");
                return field->eval_array3(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true, D(Field, eval_array3, 2))
        .def("eval_spec",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_spec()");
                check_field_output(
                    field, FieldValueType::Spectrum,
                    (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                    "Field::eval_spec()");
                return field->eval_spec(si, FieldArgs<Float>{}, active);
            },
            "si"_a, "active"_a = true, D(Field, eval_spec, 2))
        .def("eval_spec",
            [](Ptr field, const Interaction3f &it, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_spec()");
                check_field_output(
                    field, FieldValueType::Spectrum,
                    (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                    "Field::eval_spec()");
                return field->eval_spec(it, FieldArgs<Float>{}, active);
            },
            "it"_a, "active"_a = true, D(Field, eval_spec, 4))
        .def("eval_array6",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_array6()");
                check_field_output(field, FieldValueType::Features, 6,
                                   "Field::eval_array6()");
                auto result = field->eval_array6(si, FieldArgs<Float>{}, active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "si"_a, "active"_a = true, D(Field, eval_array6))
        .def("eval_array6",
            [](Ptr field, const Interaction3f &it, Mask active) {
                check_field_args_dim_zero(field, "Field::eval_array6()");
                check_field_output(field, FieldValueType::Features, 6,
                                   "Field::eval_array6()");
                auto result = field->eval_array6(it, FieldArgs<Float>{}, active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "it"_a, "active"_a = true, D(Field, eval_array6, 2))
        .def("sample_spectrum",
             [](Ptr field, const SurfaceInteraction3f &si,
                const Wavelength &sample, Mask active) {
                 return field->sample_spectrum(si, sample, active);
             },
             "si"_a, "sample"_a, "active"_a = true, D(Field, sample_spectrum))
        .def("pdf_spectrum",
             [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                 return field->pdf_spectrum(si, active);
             },
             "si"_a, "active"_a = true, D(Field, pdf_spectrum))
        .def("sample_position",
             [](Ptr field, const Point2f &sample, Mask active) {
                 return field->sample_position(sample, active);
             },
             "sample"_a, "active"_a = true, D(Field, sample_position))
        .def("pdf_position",
             [](Ptr field, const Point2f &p, Mask active) {
                 return field->pdf_position(p, active);
             },
             "p"_a, "active"_a = true, D(Field, pdf_position))
        .def("eval_1_grad",
             [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                 return field->eval_1_grad(si, active);
             },
             "si"_a, "active"_a = true, D(Field, eval_1_grad))
        .def("mean",
             [](Ptr field) { return field->mean(); }, D(Field, mean));

    if constexpr (std::is_pointer_v<Ptr>) {
        cls.def("field",
            [](Ptr field) { return field; },
            nb::rv_policy::reference)
        .def("max",
             [](Ptr field) { return field->max(); }, D(Field, max))
        .def("is_spatially_varying",
             [](Ptr field) { return field->is_spatially_varying(); },
             D(Field, is_spatially_varying))
        .def("resolution",
            [](Ptr field) -> nb::object {
                if (field->supports_interaction_queries() &&
                    !field->supports_surface_queries())
                    return nb::cast(field->resolution_3d());
                return nb::cast(field->resolution_2d());
            })
        .def("resolution_2d",
            [](Ptr field) { return field->resolution_2d(); },
            D(Field, resolution_2d))
        .def("resolution_3d",
            [](Ptr field) { return field->resolution_3d(); },
            D(Field, resolution_3d))
        .def("spectral_resolution",
            [](Ptr field) { return field->spectral_resolution(); },
            D(Field, spectral_resolution))
        .def("wavelength_range",
            [](Ptr field) { return field->wavelength_range(); },
            D(Field, wavelength_range))
        .def("bbox",
            [](Ptr field) { return field->bbox(); }, D(Field, bbox))
        .def("channel_count",
            [](Ptr field) { return field->channel_count(); },
            D(Field, channel_count))
        .def("max_per_channel",
            [](Ptr field) {
                std::vector<ScalarFloat> result(field->channel_count());
                field->max_per_channel(result.data());
                return result;
            }, D(Field, max_per_channel))
        .def("eval_gradient",
            [](Ptr field, const Interaction3f &it, Mask active) {
                return field->eval_gradient(it, active);
            },
            "it"_a, "active"_a = true, D(Field, eval_gradient))
        .def("eval_n",
            [](Ptr field, const SurfaceInteraction3f &si, Mask active) {
                uint32_t count = field->out_dim();
                std::vector<Float> result(count);
                field->eval_n(si, result.data(), count,
                              FieldArgs<Float>{}, active);
                return result;
            },
            "si"_a, "active"_a = true, D(Field, eval_n))
        .def("eval_n",
            [](Ptr field, const Interaction3f &it, Mask active) {
                uint32_t count = field->out_dim();
                std::vector<Float> result(count);
                field->eval_n(it, result.data(), count,
                              FieldArgs<Float>{}, active);
                return result;
            },
            "it"_a, "active"_a = true, D(Field, eval_n, 3))
        .def("eval",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true, D(Field, eval))
        .def("eval",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true, D(Field, eval, 2))
        .def("eval_1",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_1(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true, D(Field, eval_1))
        .def("eval_1",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_1(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true, D(Field, eval_1, 3))
        .def("eval_color3",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Color3, 3,
                                   "Field::eval_color3()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_color3(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_color3))
        .def("eval_color3",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Color3, 3,
                                   "Field::eval_color3()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_color3(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_color3, 3))
        .def("eval_array2",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Array2, 2,
                                   "Field::eval_array2()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array2(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_array2))
        .def("eval_array2",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Array2, 2,
                                   "Field::eval_array2()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array2(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_array2, 2))
        .def("eval_array3",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Array3, 3,
                                   "Field::eval_array3()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array3(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_array3))
        .def("eval_array3",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Array3, 3,
                                   "Field::eval_array3()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_array3(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_array3, 2))
        .def("eval_spec",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                check_field_output(
                    field, FieldValueType::Spectrum,
                    (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                    "Field::eval_spec()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_spec(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_spec))
        .def("eval_spec",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                check_field_output(
                    field, FieldValueType::Spectrum,
                    (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                    "Field::eval_spec()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                return field->eval_spec(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_spec, 3))
        .def("eval_array6",
            [](Ptr field, const SurfaceInteraction3f &si, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Features, 6,
                                   "Field::eval_array6()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                auto result = field->eval_array6(si, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "si"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_array6))
        .def("eval_array6",
            [](Ptr field, const Interaction3f &it, nb::object args,
               Mask active) {
                check_field_output(field, FieldValueType::Features, 6,
                                   "Field::eval_array6()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                auto result = field->eval_array6(it, FieldArgs<Float>(
                    storage.data(), (uint32_t) storage.size()), active);
                return array6_to_dynamic<Float>(std::move(result));
            },
            "it"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_array6, 2));

        cls.def("eval_n",
            [](Ptr field, const SurfaceInteraction3f &si,
               uint32_t count, nb::object args, Mask active) {
                check_field_count(field, count, "Field::eval_n()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                std::vector<Float> result(count);
                field->eval_n(si, result.data(), count,
                              FieldArgs<Float>(
                                  storage.data(), (uint32_t) storage.size()),
                              active);
                return result;
            },
            "si"_a, "count"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_n))
        .def("eval_n",
            [](Ptr field, const Interaction3f &it,
               uint32_t count, nb::object args, Mask active) {
                check_field_count(field, count, "Field::eval_n()");
                std::vector<Float> storage =
                    field_args_from_python<Float>(args, field->args_dim());
                std::vector<Float> result(count);
                field->eval_n(it, result.data(), count,
                              FieldArgs<Float>(
                                  storage.data(), (uint32_t) storage.size()),
                              active);
                return result;
            },
            "it"_a, "count"_a, "args"_a = nb::none(), "active"_a = true,
            D(Field, eval_n, 2));
    } else {
        cls.def("eval_n",
            [](Ptr field, const SurfaceInteraction3f &si,
               uint32_t count, Mask active) {
                UInt32 dim = field->out_dim(active),
                       args_dim = field->args_dim(active);
                Mask mismatch = active && (dim != count || args_dim != 0);
                if (dr::any(mismatch))
                    Throw("FieldPtr::eval_n(): count must match out_dim and "
                          "args_dim must be zero.");

                using FloatStorage = dr::DynamicArray<Float>;
                FloatStorage storage = dr::dispatch(
                    field,
                    [count](const Field *field,
                            const SurfaceInteraction3f &si,
                            Mask active) {
                        FloatStorage result = dr::zeros<FloatStorage>(count);
                        if (field->out_dim() == count && field->args_dim() == 0)
                            field->eval_n(si, result.data(), count,
                                          FieldArgs<Float>{}, active);
                        return result;
                    },
                    si, active);

                std::vector<Float> result(count);
                for (uint32_t i = 0; i < count; ++i)
                    result[i] = storage.entry(i);
                return result;
            },
            "si"_a, "count"_a, "active"_a = true, D(Field, eval_n))
        .def("eval_n",
            [](Ptr field, const Interaction3f &it,
               uint32_t count, Mask active) {
                UInt32 dim = field->out_dim(active),
                       args_dim = field->args_dim(active);
                Mask mismatch = active && (dim != count || args_dim != 0);
                if (dr::any(mismatch))
                    Throw("FieldPtr::eval_n(): count must match out_dim and "
                          "args_dim must be zero.");

                using FloatStorage = dr::DynamicArray<Float>;
                FloatStorage storage = dr::dispatch(
                    field,
                    [count](const Field *field,
                            const Interaction3f &it,
                            Mask active) {
                        FloatStorage result = dr::zeros<FloatStorage>(count);
                        if (field->out_dim() == count && field->args_dim() == 0)
                            field->eval_n(it, result.data(), count,
                                          FieldArgs<Float>{}, active);
                        return result;
                    },
                    it, active);

                std::vector<Float> result(count);
                for (uint32_t i = 0; i < count; ++i)
                    result[i] = storage.entry(i);
                return result;
            },
            "it"_a, "count"_a, "active"_a = true, D(Field, eval_n, 2));
    }
}

MI_PY_EXPORT(Field) {
    MI_PY_IMPORT_TYPES(Field, FieldPtr)
    using PyField = PyField<Float, Spectrum>;
    using Properties = mitsuba::Properties;

    auto field = nb::class_<Field, Object, PyField>(m, "Field", D(Field))
        .def(nb::init<const Properties &>(), "props"_a, D(Field, Field))
        .def_static("D65",
            nb::overload_cast<ScalarFloat>(&Field::D65),
            "scale"_a = 1.f, D(Field, D65))
        .def_static("D65",
            nb::overload_cast<ref<Field>>(&Field::D65),
            "field"_a, D(Field, D65, 2))
        .def_method(Field, out_type)
        .def_method(Field, domain)
        .def_method(Field, out_dim)
        .def_method(Field, args_dim)
        .def_method(Field, supports_scalar)
        .def_method(Field, supports_jit)
        .def_method(Field, supports_surface_queries)
        .def_method(Field, supports_interaction_queries)
        .def("__repr__", &Field::to_string);

    bind_field_generic<Field *>(field);

    if constexpr (dr::is_array_v<FieldPtr>) {
        dr::ArrayBinding b;
        auto field_ptr = dr::bind_array_t<FieldPtr>(b, m, "FieldPtr");
        bind_field_generic<FieldPtr>(field_ptr);
    }

    drjit::bind_traverse(field);
}
