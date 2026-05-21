#include <mitsuba/core/properties.h>
#include <mitsuba/render/field.h>

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

    GridField(const Properties &props) : Base(props) {
        props.mark_queried("filename");
        props.mark_queried("grid");
        props.mark_queried("data");
        props.mark_queried("filter_type");
        props.mark_queried("wrap_mode");
        props.mark_queried("raw");
        props.mark_queried("accel");
    }

    FieldValueType out_type() const override { return FieldValueType::Features; }
    FieldDomain domain() const override { return FieldDomain::Interaction; }
    uint32_t out_dim() const override { return 0; }
    uint32_t args_dim() const override { return 0; }

    bool supports_scalar() const override { return true; }
    bool supports_jit() const override { return true; }
    bool supports_surface_queries() const override { return false; }
    bool supports_interaction_queries() const override { return true; }

    // Implementations derive output metadata from the loaded storage channels.
    FloatStorage eval(const SurfaceInteraction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    FloatStorage eval(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval");
    }

    Float eval_1(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_1");
    }

    Array3f eval_array3(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_array3");
    }

    Array6f eval_array6(const Interaction3f &, Args, Mask) const override {
        NotImplementedError("eval_array6");
    }

    void eval_n(const Interaction3f &, Float *, uint32_t, Args, Mask) const override {
        NotImplementedError("eval_n");
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "GridField[]";
        return oss.str();
    }

    MI_DECLARE_CLASS(GridField)
};

MI_EXPORT_PLUGIN(GridField)
NAMESPACE_END(mitsuba)
