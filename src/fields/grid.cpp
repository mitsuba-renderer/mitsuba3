#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/field.h>
#include <mitsuba/render/volumegrid.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _field-gridfield:

Grid field (:monosp:`gridfield`)
--------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the volume grid data to be loaded

 * - grid
   - :monosp:`VolumeGrid object`
   - Existing volume grid instance used as the storage source when the field is
     created at runtime.

 * - data
   - |tensor|
   - Tensor array containing grid data. This parameter is intended for runtime
     construction from Python or C++.
   - |exposed|, |differentiable|

 * - filter_type
   - |string|
   - Interpolation mode used for normalized 3D lookups: ``trilinear``
     (default) or ``nearest``.

 * - wrap_mode
   - |string|
   - Boundary handling for coordinates outside of :math:`[0, 1]^3`:
     ``clamp`` (default), ``repeat``, or ``mirror``.

 * - raw
   - |bool|
   - Disable color-space conversion when loading grid data. Volume plugins still
     own their public color-management behavior. (Default: false)

 * - accel
   - |bool|
   - Enable hardware-accelerated grid lookups when supported by the active
     backend. (Default: true)

This plugin is the field counterpart of grid-backed volume storage. It
evaluates data at normalized 3D positions stored in a generic
:ref:`Interaction3f`. The public :monosp:`gridvolume` plugin remains
responsible for world-to-local transforms, bounding boxes, channel-count
metadata, gradients, and maximum-value queries.

The field accepts no optional argument channels. Fixed-size evaluation methods
are part of the API so volume wrappers can avoid routing scalar and 3/6-channel
lookups through dynamic storage.

*/

template <typename Float, typename Spectrum>
class GridField final : public Field<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Field)
    MI_IMPORT_TYPES()

    using FloatStorage = typename Base::FloatStorage;
    using Args         = typename Base::Args;
    using Array2f      = typename Base::Array2f;
    using Array3f      = typename Base::Array3f;
    using Array6f      = typename Base::Array6f;
    using VolumeGridType = mitsuba::VolumeGrid<Float, Spectrum>;

    GridField(const Properties &props) : Base(props) {
        std::string_view filter_type_str =
            props.get<std::string_view>("filter_type", "trilinear");
        if (filter_type_str == "nearest")
            m_filter_mode = dr::FilterMode::Nearest;
        else if (filter_type_str == "trilinear")
            m_filter_mode = dr::FilterMode::Linear;
        else
            Throw("gridfield: invalid filter_type \"%s\" (expected "
                  "\"nearest\" or \"trilinear\").", filter_type_str);

        std::string_view wrap_mode_str =
            props.get<std::string_view>("wrap_mode", "clamp");
        if (wrap_mode_str == "repeat")
            m_wrap_mode = dr::WrapMode::Repeat;
        else if (wrap_mode_str == "mirror")
            m_wrap_mode = dr::WrapMode::Mirror;
        else if (wrap_mode_str == "clamp")
            m_wrap_mode = dr::WrapMode::Clamp;
        else
            Throw("gridfield: invalid wrap_mode \"%s\" (expected "
                  "\"repeat\", \"mirror\", or \"clamp\").", wrap_mode_str);

        m_raw = props.get<bool>("raw", false);
        m_accel = props.get<bool>("accel", true);

        TensorXf tensor;
        if (props.has_property("grid")) {
            if (props.has_property("filename") || props.has_property("data"))
                Throw("gridfield: specify only one of \"grid\", "
                      "\"filename\", or \"data\".");

            ref<Object> other = props.get<ref<Object>>("grid");
            VolumeGridType *grid = dynamic_cast<VolumeGridType *>(other.get());
            if (!grid)
                Throw("gridfield: property \"grid\" must be a VolumeGrid.");
            tensor = grid_to_tensor(grid);
        } else if (props.has_property("filename")) {
            if (props.has_property("data"))
                Throw("gridfield: specify only one of \"filename\" or "
                      "\"data\".");

            fs::path path = file_resolver()->resolve(
                props.get<std::string_view>("filename"));
            if (!fs::exists(path))
                Log(Error, "\"%s\": file does not exist!", path);
            ref<VolumeGridType> grid = new VolumeGridType(path);
            tensor = grid_to_tensor(grid.get());
        } else if (props.has_property("data")) {
            tensor = props.get_any<TensorXf>("data");
        } else {
            Throw("gridfield: missing storage property \"data\", "
                  "\"grid\", or \"filename\".");
        }

        tensor = normalize_tensor_shape(tensor);
        validate_tensor(tensor);
        m_texture = Texture3f(std::move(tensor), m_accel, m_accel,
                              m_filter_mode, m_wrap_mode);
    }

    FieldValueType out_type() const override {
        switch (out_dim()) {
            case 1: return FieldValueType::Float;
            case 2: return FieldValueType::Array2;
            case 3: return FieldValueType::Array3;
            default: return FieldValueType::Features;
        }
    }

    FieldDomain domain() const override { return FieldDomain::Interaction; }
    uint32_t out_dim() const override {
        return (uint32_t) m_texture.shape()[3];
    }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return true; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override { return false; }
    bool supports_interaction_queries() const override { return true; }

    FloatStorage eval(const SurfaceInteraction3f &, Args, Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    FloatStorage eval(const Interaction3f &it, Args args,
                      Mask active) const override {
        validate_args(args);
        FloatStorage result = dr::empty<FloatStorage>(out_dim());
        eval_n(it, result.data(), out_dim(), Args{}, active);
        return result;
    }

    Float eval_1(const Interaction3f &it, Args args,
                 Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Float, 1, "eval_1");
        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        Float result;
        eval_storage(it, &result, active);
        return result;
    }

    Float eval_1(const SurfaceInteraction3f &, Args, Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    Color3f eval_color3(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Color3, 3, "eval_color3");
        if (dr::none_or<false>(active))
            return dr::zeros<Color3f>();

        Color3f result;
        eval_storage(it, result.data(), active);
        return result;
    }

    Color3f eval_color3(const SurfaceInteraction3f &, Args, Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    Array2f eval_array2(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Array2, 2, "eval_array2");
        if (dr::none_or<false>(active))
            return dr::zeros<Array2f>();

        Array2f result;
        eval_storage(it, result.data(), active);
        return result;
    }

    Array2f eval_array2(const SurfaceInteraction3f &, Args, Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    Array3f eval_array3(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Array3, 3, "eval_array3");
        if (dr::none_or<false>(active))
            return dr::zeros<Array3f>();

        Array3f result;
        eval_storage(it, result.data(), active);
        return result;
    }

    Array3f eval_array3(const SurfaceInteraction3f &, Args, Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &, Args,
                                  Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    Array6f eval_array6(const Interaction3f &it, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_feature_count(6, "eval_array6");
        if (dr::none_or<false>(active))
            return dr::zeros<Array6f>();

        Array6f result;
        eval_storage(it, result.data(), active);
        return result;
    }

    Array6f eval_array6(const SurfaceInteraction3f &, Args, Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    void eval_n(const Interaction3f &it, Float *out, uint32_t count,
                Args args, Mask active) const override {
        validate_args(args);
        if (count != out_dim())
            Throw("gridfield::eval_n(): count (%u) must match out_dim (%u).",
                  count, out_dim());

        if (dr::none_or<false>(active)) {
            for (uint32_t i = 0; i < count; ++i)
                out[i] = dr::zeros<Float>();
            return;
        }

        eval_storage(it, out, active);
    }

    void eval_n(const SurfaceInteraction3f &, Float *, uint32_t,
                Args, Mask) const override {
        Throw("gridfield: domain mismatch, expected Interaction queries, "
              "got Surface interaction.");
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("data", m_texture.tensor(), ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "data")) {
            validate_tensor(m_texture.tensor());
            m_texture.update_inplace();
            sync_ad_texture_state();
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "GridField[" << std::endl
            << "  resolution = [" << m_texture.shape()[2] << ", "
            << m_texture.shape()[1] << ", " << m_texture.shape()[0]
            << "]," << std::endl
            << "  channels = " << out_dim() << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(GridField)

private:
    TensorXf grid_to_tensor(VolumeGridType *grid) const {
        ScalarVector3u res = grid->size();
        size_t shape[4] = {
            (size_t) res.z(),
            (size_t) res.y(),
            (size_t) res.x(),
            grid->channel_count()
        };
        return TensorXf(grid->data(), 4, shape);
    }

    TensorXf normalize_tensor_shape(const TensorXf &tensor) const {
        if (tensor.ndim() == 4)
            return tensor;

        if (tensor.ndim() != 3)
            Throw("gridfield: data Tensor rank/dimension mismatch "
                  "(got %zu, expected 3 or 4).", tensor.ndim());

        size_t shape[4] = {
            tensor.shape(0),
            tensor.shape(1),
            tensor.shape(2),
            1
        };
        // Three-dimensional tensors represent scalar grids.
        return TensorXf(tensor.array(), 4, shape);
    }

    void validate_tensor(const TensorXf &tensor) const {
        if (tensor.ndim() != 4)
            Throw("gridfield: data Tensor rank/dimension mismatch "
                  "(got %zu, expected 4).", tensor.ndim());
        if (tensor.shape(3) == 0)
            Throw("gridfield: data Tensor must have at least one channel.");
    }

    void validate_args(Args args) const {
        if (args.size != 0)
            Throw("gridfield: args_dim is 0, got %u argument channel(s).",
                  args.size);
    }

    void validate_output(FieldValueType expected_type, uint32_t expected_dim,
                         const char *method) const {
        if (out_type() != expected_type || out_dim() != expected_dim)
            Throw("gridfield::%s(): incompatible field metadata "
                  "(out_type=%s, out_dim=%u).",
                  method, field_value_type_name(out_type()), out_dim());
    }

    void validate_feature_count(uint32_t count, const char *method) const {
        if (out_type() != FieldValueType::Features || out_dim() != count)
            Throw("gridfield::%s(): expected Features with out_dim=%u, "
                  "got out_type=%s and out_dim=%u.",
                  method, count, field_value_type_name(out_type()), out_dim());
    }

    const char *field_value_type_name(FieldValueType type) const {
        switch (type) {
            case FieldValueType::Float: return "Float";
            case FieldValueType::Spectrum: return "Spectrum";
            case FieldValueType::Color3: return "Color3";
            case FieldValueType::Array2: return "Array2";
            case FieldValueType::Array3: return "Array3";
            case FieldValueType::Features: return "Features";
            default: return "Unknown";
        }
    }

    MI_INLINE void eval_storage(const Interaction3f &it, Float *out,
                                Mask active) const {
        MI_MASK_ARGUMENT(active);
        sync_ad_texture();
        if (m_accel)
            m_texture.template eval<Float>(it.p, out, active);
        else
            m_texture.template eval_nonaccel<Float>(it.p, out, active);
    }

    void sync_ad_texture() const {
        if constexpr (dr::is_diff_v<Float>) {
            /* See BitmapField::sync_ad_texture(). Grid fields expose the
               unpadded tensor through traversal while lookup uses the texture's
               internal storage. */
            if (!m_ad_texture_synced &&
                dr::grad_enabled(m_texture.tensor().array())) {
                const_cast<Texture3f &>(m_texture).update_inplace();
                m_ad_texture_synced = true;
            }
        }
    }

    void sync_ad_texture_state() {
        if constexpr (dr::is_diff_v<Float>)
            m_ad_texture_synced = dr::grad_enabled(m_texture.tensor().array());
    }

private:
    Texture3f m_texture;
    dr::FilterMode m_filter_mode;
    dr::WrapMode m_wrap_mode;
    bool m_raw = false;
    bool m_accel = true;
    mutable bool m_ad_texture_synced = false;
};

MI_EXPORT_PLUGIN(GridField)
NAMESPACE_END(mitsuba)
