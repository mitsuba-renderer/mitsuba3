#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-bumpmap:

Bump map BSDF adapter (:monosp:`bumpmap`)
-----------------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - |texture|
   - Specifies the bump map texture.
   - |exposed|, |differentiable|, |discontinuous|

 * - (Nested plugin)
   - |bsdf|
   - A BSDF model that should be affected by the bump map
   - |exposed|, |differentiable|, |discontinuous|

 * - scale
   - |float|
   - Bump map gradient multiplier. (Default: 1.0)
   - |exposed|

Bump mapping is a simple technique for cheaply adding surface detail to a rendering. This is done
by perturbing the shading coordinate frame based on a displacement height field provided as a
texture. This method can lend objects a highly realistic and detailed appearance (e.g. wrinkled
or covered by scratches and other imperfections) without requiring any changes to the input geometry.
The implementation in Mitsuba uses the common approach of ignoring the usually negligible
texture-space derivative of the base mesh surface normal. As side effect of this decision,
it is invariant to constant offsets in the height field texture: only variations in its luminance
cause changes to the shading frame.

Note that the magnitude of the height field variations influences the scale of the displacement.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_bumpmap_without.jpg
   :caption: Roughplastic BSDF
.. subfigure:: ../../resources/data/docs/images/render/bsdf_bumpmap_with.jpg
   :caption: Roughplastic BSDF with bump mapping
.. subfigend::
   :label: fig-bumpmap


The following XML snippet describes a rough plastic material affected by a bump
map. Note the we set the ``raw`` properties of the bump map ``bitmap`` object to
``true`` in order to disable the transformation from sRGB to linear encoding:

.. tabs::
    .. code-tab:: xml
        :name: bumpmap

        <bsdf type="bumpmap">
            <texture name="arbitrary" type="bitmap">
                <boolean name="raw" value="true"/>
                <string name="filename" value="textures/bumpmap.jpg"/>
            </texture>
            <bsdf type="roughplastic"/>
        </bsdf>

    .. code-tab:: python

        'type': 'bumpmap',
        'arbitrary': {
            'raw': True,
            'filename': 'textures/bumpmap.jpg'
        },
        'bsdf': {
            'type': 'roughplastic'
        }
*/

template <typename Float, typename Spectrum>
class BumpMap final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    BumpMap(const Properties &props) : Base(props) {
        for (auto &[name, obj] : props.objects(false)) {
            auto bsdf = dynamic_cast<Base *>(obj.get());
            if (bsdf) {
                if (m_nested_bsdf)
                    Throw("Only a single BSDF child object can be specified.");
                m_nested_bsdf = bsdf;
                props.mark_queried(name);
            }
            auto texture = dynamic_cast<Texture *>(obj.get());
            if (texture) {
                if (m_nested_texture)
                    Throw("Only a single Texture child object can be specified.");
                m_nested_texture = texture;
                props.mark_queried(name);
            }
        }
        if (!m_nested_bsdf)
            Throw("Exactly one BSDF child object must be specified.");
        if (!m_nested_texture)
            Throw("Exactly one Texture child object must be specified.");

        m_scale = props.get<ScalarFloat>("scale", 1.f);

        // Add all nested components
        m_components.clear();
        for (size_t i = 0; i < m_nested_bsdf->component_count(); ++i)
            m_components.push_back(m_nested_bsdf->flags(i));
        m_flags = m_nested_bsdf->flags();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("nested_bsdf",     m_nested_bsdf.get(),    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_object("nested_texture",  m_nested_texture.get(), ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_parameter("scale",        m_scale,                +ParamFlags::NonDifferentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        // Sample nested BSDF with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi = perturbed_si.to_local(si.wi);
        auto [bs, weight] = m_nested_bsdf->sample(ctx, perturbed_si,
                                                  sample1, sample2, active);
        active &= dr::any(unpolarized_spectrum(weight) != 0.f);

        // Transform sampled 'wo' back to original frame and check orientation
        Vector3f perturbed_wo = perturbed_si.to_world(bs.wo);
        active &= Frame3f::cos_theta(bs.wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;
        bs.wo = perturbed_wo;

        return { bs, weight & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        // Evaluate nested BSDF with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi       = perturbed_si.to_local(si.wi);
        Vector3f perturbed_wo = perturbed_si.to_local(wo);

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        return dr::select(active, m_nested_bsdf->eval(ctx, perturbed_si, perturbed_wo, active), 0.f);
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        // Evaluate nested BSDF pdf with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi       = perturbed_si.to_local(si.wi);
        Vector3f perturbed_wo = perturbed_si.to_local(wo);

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        return dr::select(active, m_nested_bsdf->pdf(ctx, perturbed_si, perturbed_wo, active), 0.f);
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        // Evaluate nested BSDF with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi       = perturbed_si.to_local(si.wi);
        Vector3f perturbed_wo = perturbed_si.to_local(wo);

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        auto [value, pdf] = m_nested_bsdf->eval_pdf(ctx, perturbed_si, perturbed_wo, active);
        return { value & active, dr::select(active, pdf, 0.f) };
    }

    Frame3f frame(const SurfaceInteraction3f &si, Mask active) const {
        // Evaluate texture gradient
        Vector2f grad_uv = m_scale * m_nested_texture->eval_1_grad(si, active);

        // Compute perturbed differential geometry
        Vector3f dp_du = dr::fmadd(si.sh_frame.n, grad_uv.x() - dr::dot(si.sh_frame.n, si.dp_du), si.dp_du);
        Vector3f dp_dv = dr::fmadd(si.sh_frame.n, grad_uv.y() - dr::dot(si.sh_frame.n, si.dp_dv), si.dp_dv);

        // Bump-mapped shading normal
        Frame3f result;
        result.n = dr::normalize(dr::cross(dp_du, dp_dv));

        // Flip if not aligned with geometric normal
        result.n[dr::dot(si.n, result.n) < .0f] *= -1.f;

        // Convert to small rotation from original shading frame
        result.n = si.to_local(result.n);

        // Gram-schmidt orthogonalization to compute local shading frame
        result.s = dr::normalize(dr::fnmadd(result.n, dr::dot(result.n, si.dp_du), si.dp_du));
        result.t = dr::cross(result.n, result.s);

        return result;
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        return m_nested_bsdf->eval_diffuse_reflectance(si, active);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BumpMap[" << std::endl
            << "  nested_bsdf = " << string::indent(m_nested_bsdf) << std::endl
            << "  nested_texture = " << string::indent(m_nested_texture) << "," << std::endl
            << "  scale = " << string::indent(m_scale) << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    ScalarFloat m_scale;
    ref<Texture> m_nested_texture;
    ref<Base> m_nested_bsdf;
};

MI_IMPLEMENT_CLASS_VARIANT(BumpMap, BSDF)
MI_EXPORT_PLUGIN(BumpMap, "Bump map material adapter")
NAMESPACE_END(mitsuba)
