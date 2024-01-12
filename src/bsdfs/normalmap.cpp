#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-normalmap:

Normal map BSDF (:monosp:`normalmap`)
-------------------------------------

.. pluginparameters::

 * - normalmap
   - |texture|
   - The color values of this texture specify the perturbed normals relative in the local surface coordinate system
   - |exposed|, |differentiable|, |discontinuous|

 * - (Nested plugin)
   - |bsdf|
   - A BSDF model that should be affected by the normal map
   - |exposed|, |differentiable|

Normal mapping is a simple technique for cheaply adding surface detail to a rendering. This is done
by perturbing the shading coordinate frame based on a normal map provided as a texture. This method
can lend objects a highly realistic and detailed appearance (e.g. wrinkled or covered by scratches
and other imperfections) without requiring any changes to the input geometry.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_normalmap_without.jpg
   :caption: Roughplastic BSDF
.. subfigure:: ../../resources/data/docs/images/render/bsdf_normalmap_with.jpg
   :caption: Roughplastic BSDF with normal mapping
.. subfigend::
   :label: fig-normalmap

A normal map is a RGB texture, whose color channels encode the XYZ coordinates of the desired
surface normals. These are specified **relative** to the local shading frame, which means that a
normal map with a value of :math:`(0,0,1)` everywhere causes no changes to the surface. To turn the
3D normal directions into (nonnegative) color values suitable for this plugin, the mapping
:math:`x \mapsto (x+1)/2` must be applied to each component.

The following XML snippet describes a smooth mirror material affected by a normal map. Note the we set the
``raw`` properties of the normal map ``bitmap`` object to ``true`` in order to disable the
transformation from sRGB to linear encoding:

.. tabs::
    .. code-tab:: xml
        :name: normalmap

        <bsdf type="normalmap">
            <texture name="normalmap" type="bitmap">
                <boolean name="raw" value="true"/>
                <string name="filename" value="textures/normalmap.jpg"/>
            </texture>
            <bsdf type="roughplastic"/>
        </bsdf>

    .. code-tab:: python

        'type': 'normalmap',
        'normalmap': {
            'type': 'bitmap',
            'raw': True,
            'filename': 'textures/normalmap.jpg'
        },
        'bsdf': {
            'type': 'roughplastic'
        }
*/

template <typename Float, typename Spectrum>
class NormalMap final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    NormalMap(const Properties &props) : Base(props) {
        for (auto &[name, obj] : props.objects(false)) {
            auto bsdf = dynamic_cast<Base *>(obj.get());

            if (bsdf) {
                if (m_nested_bsdf)
                    Throw("Only a single BSDF child object can be specified.");
                m_nested_bsdf = bsdf;
                props.mark_queried(name);
            }
        }
        if (!m_nested_bsdf)
            Throw("Exactly one BSDF child object must be specified.");

        // TODO: How to assert this is actually a RGBDataTexture?
        m_normalmap = props.texture<Texture>("normalmap");

        // Add all nested components
        m_flags = (uint32_t) 0;
        for (size_t i = 0; i < m_nested_bsdf->component_count(); ++i) {
            m_components.push_back((m_nested_bsdf->flags(i)));
            m_flags |= m_components.back();
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("nested_bsdf", m_nested_bsdf.get(), +ParamFlags::Differentiable);
        callback->put_object("normalmap",   m_normalmap.get(),   ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        // Sample nested BSDF with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi = perturbed_si.to_local(si.wi);
        auto [bs, weight] = m_nested_bsdf->sample(ctx, perturbed_si,
                                                  sample1, sample2, active);
        active &= dr::any(unpolarized_spectrum(weight) != 0.f);
        if (dr::none_or<false>(active))
            return { bs, 0.f };

        // Transform sampled 'wo' back to original frame and check orientation
        Vector3f perturbed_wo = perturbed_si.to_world(bs.wo);
        active &= Frame3f::cos_theta(bs.wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;
        bs.wo = perturbed_wo;

        return { bs, weight & active };
    }


    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        // Evaluate nested BSDF with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi       = perturbed_si.to_local(si.wi);
        Vector3f perturbed_wo = perturbed_si.to_local(wo);

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        return m_nested_bsdf->eval(ctx, perturbed_si, perturbed_wo, active) & active;
    }


    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        // Evaluate nested BSDF with perturbed shading frame
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
        Normal3f n = dr::fmadd(m_normalmap->eval_3(si, active), 2, -1.f);

        Frame3f result;
        result.n = dr::normalize(n);
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
        oss << "NormalMap[" << std::endl
            << "  nested_bsdf = " << string::indent(m_nested_bsdf) << ","
            << std::endl
            << "  normalmap = " << string::indent(m_normalmap) << ","
            << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    ref<Base> m_nested_bsdf;
    ref<Texture> m_normalmap;
};

MI_IMPLEMENT_CLASS_VARIANT(NormalMap, BSDF)
MI_EXPORT_PLUGIN(NormalMap, "Normal map material adapter");
NAMESPACE_END(mitsuba)
