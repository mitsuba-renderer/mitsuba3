#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

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
        props.mark_queried("filename");
        props.mark_queried("bitmap");
        props.mark_queried("data");
        props.mark_queried("filter_type");
        props.mark_queried("wrap_mode");
        props.mark_queried("raw");
        props.mark_queried("accel");
        props.mark_queried("format");
    }

    FieldValueType out_type() const override { return FieldValueType::Features; }
    FieldDomain domain() const override { return FieldDomain::Surface; }
    uint32_t out_dim() const override { return 0; }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return true; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override { return true; }
    bool supports_interaction_queries() const override { return false; }

    // Implementations derive output metadata from the loaded storage channels.
    FloatStorage eval(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    FloatStorage eval(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    Float eval_1(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_1");
    }

    Color3f eval_color3(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_color3");
    }

    Array2f eval_array2(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_array2");
    }

    Array3f eval_array3(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_array3");
    }

    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval_spec");
    }

    void eval_n(const SurfaceInteraction3f &, Float *, uint32_t, Args, Mask) const override {
        NotImplementedError("eval_n");
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BitmapField[]";
        return oss.str();
    }

    MI_DECLARE_CLASS(BitmapField)
};

MI_EXPORT_PLUGIN(BitmapField)
NAMESPACE_END(mitsuba)
