#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/field.h>
#include <drjit/tensor.h>
#include <drjit/texture.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _field-bitmapfield:

Bitmap field (:monosp:`bitmapfield`)
------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the bitmap data to be loaded

 * - bitmap
   - :monosp:`Bitmap object`
   - Existing bitmap instance used as the storage source when the field is
     created at runtime.

 * - data
   - |tensor|
   - Tensor array containing field data. This parameter is intended for runtime
     construction from Python or C++.
   - |exposed|, |differentiable|

 * - filter_type
   - |string|
   - Interpolation mode used for UV lookups: ``bilinear`` (default) or
     ``nearest``.

 * - wrap_mode
   - |string|
   - Boundary handling for UV coordinates outside of :math:`[0, 1]^2`:
     ``repeat`` (default), ``mirror``, or ``clamp``.

 * - raw
   - |bool|
   - Disable color-space conversion when loading image data. Texture plugins
     still own their public color-management behavior. (Default: false)

 * - format
   - |string|
   - Storage format hint shared with bitmap-backed textures. The implementation
     should preserve the texture plugin's existing ``auto``, ``variant``, and
     ``fp16`` behavior.

 * - accel
   - |bool|
   - Enable hardware-accelerated texture lookups when supported by the active
     backend. (Default: true)

This plugin is the field counterpart of bitmap-backed texture storage. It
evaluates data at the UV coordinates of a :ref:`SurfaceInteraction3f` and does
not expose texture-specific operations such as position sampling, mean/max
queries, gradients, or spectral upsampling policy.

The field accepts no optional argument channels. Public texture plugins that
use this storage internally must continue to apply their existing texture
mapping, color-processing, and sampling semantics outside of this plugin.

*/

template <typename Float, typename Spectrum>
class BitmapField final : public Field<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Field)
    MI_IMPORT_TYPES()

    using FloatStorage = typename Base::FloatStorage;
    using Args         = typename Base::Args;
    using Array2f      = typename Base::Array2f;
    using Array3f      = typename Base::Array3f;
    using Array6f      = typename Base::Array6f;

    BitmapField(const Properties &props) : Base(props) {
        props.mark_queried("format");

        std::string_view filter_mode_str =
            props.get<std::string_view>("filter_type", "bilinear");
        if (filter_mode_str == "nearest")
            m_filter_mode = dr::FilterMode::Nearest;
        else if (filter_mode_str == "bilinear")
            m_filter_mode = dr::FilterMode::Linear;
        else
            Throw("bitmapfield: invalid filter_type \"%s\" (expected "
                  "\"nearest\" or \"bilinear\").", filter_mode_str);

        std::string_view wrap_mode_str =
            props.get<std::string_view>("wrap_mode", "repeat");
        if (wrap_mode_str == "repeat")
            m_wrap_mode = dr::WrapMode::Repeat;
        else if (wrap_mode_str == "mirror")
            m_wrap_mode = dr::WrapMode::Mirror;
        else if (wrap_mode_str == "clamp")
            m_wrap_mode = dr::WrapMode::Clamp;
        else
            Throw("bitmapfield: invalid wrap_mode \"%s\" (expected "
                  "\"repeat\", \"mirror\", or \"clamp\").", wrap_mode_str);

        m_raw = props.get<bool>("raw", false);
        m_accel = props.get<bool>("accel", true);

        TensorXf tensor;
        if (props.has_property("bitmap")) {
            if (props.has_property("filename") || props.has_property("data"))
                Throw("bitmapfield: specify only one of \"bitmap\", "
                      "\"filename\", or \"data\".");

            ref<Object> other = props.get<ref<Object>>("bitmap");
            Bitmap *bitmap = dynamic_cast<Bitmap *>(other.get());
            if (!bitmap)
                Throw("bitmapfield: property \"bitmap\" must be a Bitmap.");

            tensor = bitmap_to_tensor(bitmap);
        } else if (props.has_property("filename")) {
            if (props.has_property("data"))
                Throw("bitmapfield: specify only one of \"filename\" or "
                      "\"data\".");

            fs::path path = file_resolver()->resolve(
                props.get<std::string_view>("filename"));
            ref<Bitmap> bitmap = new Bitmap(path);
            tensor = bitmap_to_tensor(bitmap.get());
        } else if (props.has_property("data")) {
            tensor = props.get_any<TensorXf>("data");
        } else {
            Throw("bitmapfield: missing storage property \"data\", "
                  "\"bitmap\", or \"filename\".");
        }

        validate_tensor(tensor);
        m_texture = Texture2f(std::move(tensor), m_accel, m_accel,
                              m_filter_mode, m_wrap_mode);
    }

    FieldValueType out_type() const override {
        switch (out_dim()) {
            case 1: return FieldValueType::Float;
            case 2: return FieldValueType::Array2;
            case 3: return FieldValueType::Color3;
            default: return FieldValueType::Features;
        }
    }

    FieldDomain domain() const override { return FieldDomain::Surface; }
    uint32_t out_dim() const override {
        return (uint32_t) m_texture.shape()[2];
    }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return true; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override { return true; }
    bool supports_interaction_queries() const override { return false; }

    FloatStorage eval(const SurfaceInteraction3f &si, Args args,
                      Mask active) const override {
        validate_args(args);
        FloatStorage result = dr::empty<FloatStorage>(out_dim());
        eval_n(si, result.data(), out_dim(), Args{}, active);
        return result;
    }

    FloatStorage eval(const Interaction3f &, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
    }

    Float eval_1(const SurfaceInteraction3f &si, Args args,
                 Mask active) const override {
        validate_args(args);
        if (out_type() != FieldValueType::Float || out_dim() != 1) {
            /* Match bitmap texture semantics: RGB storage can still be queried
               as a scalar by returning luminance. Arbitrary feature fields are
               rejected below. */
            if (out_type() != FieldValueType::Color3 || out_dim() != 3)
                validate_output(FieldValueType::Float, 1, "eval_1");

            return luminance(eval_color3(si, args, active));
        }

        if (dr::none_or<false>(active))
            return dr::zeros<Float>();

        Float result;
        eval_storage(si, &result, active);
        return result;
    }

    Float eval_1(const Interaction3f &, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
    }

    Color3f eval_color3(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Color3, 3, "eval_color3");
        if (dr::none_or<false>(active))
            return dr::zeros<Color3f>();

        Color3f result;
        eval_storage(si, result.data(), active);
        return result;
    }

    Color3f eval_color3(const Interaction3f &, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
    }

    Array2f eval_array2(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Array2, 2, "eval_array2");
        if (dr::none_or<false>(active))
            return dr::zeros<Array2f>();

        Array2f result;
        eval_storage(si, result.data(), active);
        return result;
    }

    Array2f eval_array2(const Interaction3f &, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
    }

    Array3f eval_array3(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Array3, 3, "eval_array3");
        if (dr::none_or<false>(active))
            return dr::zeros<Array3f>();

        Array3f result;
        eval_storage(si, result.data(), active);
        return result;
    }

    Array3f eval_array3(const Interaction3f &, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
    }

    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si, Args args,
                                  Mask active) const override {
        validate_args(args);
        validate_output(FieldValueType::Spectrum,
                        (uint32_t) dr::size_v<UnpolarizedSpectrum>,
                        "eval_spec");
        if (dr::none_or<false>(active))
            return dr::zeros<UnpolarizedSpectrum>();

        UnpolarizedSpectrum result;
        eval_storage(si, result.data(), active);
        return result;
    }

    UnpolarizedSpectrum eval_spec(const Interaction3f &, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
    }

    Array6f eval_array6(const SurfaceInteraction3f &si, Args args,
                        Mask active) const override {
        validate_args(args);
        validate_feature_count(6, "eval_array6");
        if (dr::none_or<false>(active))
            return dr::zeros<Array6f>();

        Array6f result;
        eval_storage(si, result.data(), active);
        return result;
    }

    Array6f eval_array6(const Interaction3f &, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
    }

    void eval_n(const SurfaceInteraction3f &si, Float *out, uint32_t count,
                Args args, Mask active) const override {
        validate_args(args);
        if (count != out_dim())
            Throw("bitmapfield::eval_n(): count (%u) must match out_dim (%u).",
                  count, out_dim());

        if (dr::none_or<false>(active)) {
            for (uint32_t i = 0; i < count; ++i)
                out[i] = dr::zeros<Float>();
            return;
        }

        eval_storage(si, out, active);
    }

    void eval_n(const Interaction3f &, Float *, uint32_t, Args, Mask) const override {
        Throw("bitmapfield: domain mismatch, expected Surface interaction "
              "queries, got Interaction.");
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
        oss << "BitmapField[" << std::endl
            << "  resolution = [" << m_texture.shape()[1] << ", "
            << m_texture.shape()[0] << "]," << std::endl
            << "  channels = " << out_dim() << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(BitmapField)

private:
    TensorXf bitmap_to_tensor(Bitmap *bitmap) const {
        Bitmap::PixelFormat pixel_format = bitmap->pixel_format();
        switch (pixel_format) {
            case Bitmap::PixelFormat::Y:
            case Bitmap::PixelFormat::YA:
                pixel_format = Bitmap::PixelFormat::Y;
                break;

            case Bitmap::PixelFormat::RGB:
            case Bitmap::PixelFormat::RGBA:
            case Bitmap::PixelFormat::XYZ:
            case Bitmap::PixelFormat::XYZA:
                pixel_format = Bitmap::PixelFormat::RGB;
                break;

            default:
                Throw("bitmapfield: unsupported bitmap pixel format "
                      "(expected Y[A], RGB[A], or XYZ[A]).");
        }

        if (m_raw)
            bitmap->set_srgb_gamma(false);

        ref<Bitmap> converted =
            bitmap->convert(pixel_format, Struct::Type::Float32, false);
        ScalarVector2i res = ScalarVector2i(converted->size());
        size_t shape[3] = {
            (size_t) res.y(),
            (size_t) res.x(),
            converted->channel_count()
        };
        return TensorXf(converted->data(), 3, shape);
    }

    void validate_tensor(const TensorXf &tensor) const {
        if (tensor.ndim() != 3)
            Throw("bitmapfield: data Tensor rank/dimension mismatch "
                  "(got %zu, expected 3).", tensor.ndim());
        if (tensor.shape(2) == 0)
            Throw("bitmapfield: data Tensor must have at least one channel.");
    }

    void validate_args(Args args) const {
        if (args.size != 0)
            Throw("bitmapfield: args_dim is 0, got %u argument channel(s).",
                  args.size);
    }

    void validate_output(FieldValueType expected_type, uint32_t expected_dim,
                         const char *method) const {
        if (out_type() != expected_type || out_dim() != expected_dim)
            Throw("bitmapfield::%s(): incompatible field metadata "
                  "(out_type=%s, out_dim=%u).",
                  method, field_value_type_name(out_type()), out_dim());
    }

    void validate_feature_count(uint32_t count, const char *method) const {
        if (out_type() != FieldValueType::Features || out_dim() != count)
            Throw("bitmapfield::%s(): expected Features with out_dim=%u, "
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

    MI_INLINE void eval_storage(const SurfaceInteraction3f &si, Float *out,
                                Mask active) const {
        MI_MASK_ARGUMENT(active);
        sync_ad_texture();
        m_texture.template eval<Float>(si.uv, out, active);
    }

    void sync_ad_texture() const {
        if constexpr (dr::is_diff_v<Float>) {
            /* Traversal exposes Texture::tensor() as the differentiable
               parameter. When gradients are enabled on that tensor without an
               intervening parameters_changed() call, refresh the padded texture
               storage before evaluating so the lookup depends on it. */
            if (!m_ad_texture_synced &&
                dr::grad_enabled(m_texture.tensor().array())) {
                const_cast<Texture2f &>(m_texture).update_inplace();
                m_ad_texture_synced = true;
            }
        }
    }

    void sync_ad_texture_state() {
        if constexpr (dr::is_diff_v<Float>)
            m_ad_texture_synced = dr::grad_enabled(m_texture.tensor().array());
    }

private:
    Texture2f m_texture;
    dr::FilterMode m_filter_mode;
    dr::WrapMode m_wrap_mode;
    bool m_raw = false;
    bool m_accel = true;
    mutable bool m_ad_texture_synced = false;
};

MI_EXPORT_PLUGIN(BitmapField)
NAMESPACE_END(mitsuba)
