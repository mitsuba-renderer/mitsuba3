#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/field.h>

#include <sstream>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-neuralbsdf:

Neural diffuse material (:monosp:`neuralbsdf`)
----------------------------------------------

.. pluginparameters::

 * - reflectance
   - |field|
   - Field that predicts the diffuse reflectance or albedo. Supported field
     outputs are ``Float``, ``Color3``, and ``Spectrum``. The field may accept
     either no optional arguments or the BSDF argument layout described below.
   - |exposed|, |differentiable|

 * - mode
   - |string|
   - Scattering model exposed by this plugin. The initial public mode is a
     field-modulated diffuse model. More expressive modes must define matching
     value, sampling, and PDF outputs before becoming public.

The neural BSDF owns scattering semantics while fields provide learned or
tabulated reflectance data. The initial model is intentionally a diffuse BSDF:
it uses cosine-hemisphere sampling, the cosine PDF, and the same front-side
diffuse convention as :monosp:`diffuse`.

When the reflectance field declares optional argument channels, the BSDF passes
the following 11 values in order:

.. list-table::
   :widths: 10 30
   :header-rows: 1

   * - Channels
     - Meaning
   * - 0-2
     - Surface position ``si.p``
   * - 3-4
     - Surface UV coordinates ``si.uv``
   * - 5-7
     - Incident direction ``si.wi`` in the local shading frame
   * - 8-10
     - Outgoing direction ``wo`` in the local shading frame

Fields with any other nonzero argument dimension must be rejected by the
implementation. A field that predicts arbitrary scattering values is not enough
to implement consistent ``sample()`` and ``pdf()`` methods and must not be
accepted by this initial plugin mode.

*/

template <typename Float, typename Spectrum>
class NeuralBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Field)

    using Args = typename Field::Args;

    NeuralBSDF(const Properties &props) : Base(props) {
        m_reflectance = props.get<ref<Field>>("reflectance");
        props.mark_queried("mode");

        m_flags = BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &,
                                             const SurfaceInteraction3f &,
                                             Float,
                                             const Point2f &,
                                             Mask) const override {
        NotImplementedError("sample");
    }

    Spectrum eval(const BSDFContext &, const SurfaceInteraction3f &,
                  const Vector3f &, Mask) const override {
        NotImplementedError("eval");
    }

    Float pdf(const BSDFContext &, const SurfaceInteraction3f &,
              const Vector3f &, Mask) const override {
        NotImplementedError("pdf");
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &,
                                        const SurfaceInteraction3f &,
                                        const Vector3f &,
                                        Mask) const override {
        NotImplementedError("eval_pdf");
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &,
                                      Mask) const override {
        NotImplementedError("eval_diffuse_reflectance");
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("reflectance", m_reflectance, ParamFlags::Differentiable);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "NeuralBSDF[" << std::endl
            << "  reflectance = " << m_reflectance << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(NeuralBSDF)

private:
    static constexpr uint32_t ArgsDim = 11;

    Args field_args(const SurfaceInteraction3f &, const Vector3f &) const {
        NotImplementedError("field_args");
    }

private:
    ref<Field> m_reflectance;

    MI_TRAVERSE_CB(Base, m_reflectance)
};

MI_EXPORT_PLUGIN(NeuralBSDF)
NAMESPACE_END(mitsuba)
