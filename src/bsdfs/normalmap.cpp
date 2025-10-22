#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

#include "normalmap_helpers.h"

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

 * - flip_invalid_normals
   - |bool|
   - If enabled, the plugin will ensure that the perturbed normals are always
     consistent with the geometric normal. This prevents visual artifacts and is
     achieved by a simply flipping the shading normal, as described in
     :cite:`Schuessler2017Microfacet`. (Default: true)
   - |exposed|

 * - use_shadowing_function
   - |bool|
   - If enabled, the plugin uses a Microfacet-based shadowing term
     :cite:`Estevez2019` to smooth out transitions on shadow boundaries. (Default: true)
   - |exposed|

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

The following XML snippet describes a smooth mirror material affected by a normal map. Note that we set the
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
        for (auto &prop : props.objects()) {
            if (Base *bsdf = prop.try_get<Base>()) {
                if (m_nested_bsdf)
                    Throw("Only a single BSDF child object can be specified.");
                m_nested_bsdf = bsdf;
            }
        }
        if (!m_nested_bsdf)
            Throw("Exactly one BSDF child object must be specified.");

        // TODO: How to assert this is actually a RGBDataTexture?
        m_normalmap = props.get_texture<Texture>("normalmap");

        m_flip_invalid_normals = props.get<bool>("flip_invalid_normals", true);
        m_use_shadowing_function = props.get<bool>("use_shadowing_function", true);

        // Add all nested components
        m_flags = (uint32_t) 0;
        for (size_t i = 0; i < m_nested_bsdf->component_count(); ++i) {
            m_components.push_back((m_nested_bsdf->flags(i)));
            m_flags |= m_components.back();
        }
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("nested_bsdf", m_nested_bsdf, ParamFlags::Differentiable);
        cb->put("normalmap",   m_normalmap,   ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        // Sample nested BSDF with perturbed shading frame
        auto [ perturbed_frame_wrt_si, perturbed_frame_wrt_world ] = frame(si, active);
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = perturbed_frame_wrt_world;
        perturbed_si.wi       = perturbed_frame_wrt_si.to_local(si.wi);
        auto [bs, weight] = m_nested_bsdf->sample(ctx, perturbed_si,
                                                  sample1, sample2, active);
        active &= dr::any(unpolarized_spectrum(weight) != 0.f);
        if (dr::none_or<false>(active))
            return { bs, 0.f };

        // Transform sampled 'wo' back to original frame and check orientation
        Vector3f perturbed_wo = perturbed_frame_wrt_si.to_world(bs.wo);
        active &= Frame3f::cos_theta(bs.wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;
        bs.pdf = dr::select(active, bs.pdf, 0.f);
        bs.wo = perturbed_wo;

        if (m_use_shadowing_function)
            weight *= eval_shadow_terminator(perturbed_frame_wrt_si.n, bs.wo);
        return { bs, weight & active };
    }


    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        // Evaluate nested BSDF with perturbed shading frame
        auto [ perturbed_frame_wrt_si, perturbed_frame_wrt_world ] = frame(si, active);
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = perturbed_frame_wrt_world;
        perturbed_si.wi       = perturbed_frame_wrt_si.to_local(si.wi);
        Vector3f perturbed_wo = perturbed_frame_wrt_si.to_local(wo);

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        Spectrum value = m_nested_bsdf->eval(ctx, perturbed_si, perturbed_wo, active);

        if (m_use_shadowing_function)
            value *= eval_shadow_terminator(perturbed_frame_wrt_si.n, wo);
        return value & active;
    }


    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        // Evaluate nested BSDF with perturbed shading frame
        auto [ perturbed_frame_wrt_si, perturbed_frame_wrt_world ] = frame(si, active);
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = perturbed_frame_wrt_world;
        perturbed_si.wi       = perturbed_frame_wrt_si.to_local(si.wi);
        Vector3f perturbed_wo = perturbed_frame_wrt_si.to_local(wo);

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
        auto [ perturbed_frame_wrt_si, perturbed_frame_wrt_world ] = frame(si, active);
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = perturbed_frame_wrt_world;
        perturbed_si.wi       = perturbed_frame_wrt_si.to_local(si.wi);
        Vector3f perturbed_wo = perturbed_frame_wrt_si.to_local(wo);

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        auto [value, pdf] = m_nested_bsdf->eval_pdf(ctx, perturbed_si, perturbed_wo, active);

        if (m_use_shadowing_function)
            value *= eval_shadow_terminator(perturbed_frame_wrt_si.n, wo);
        return { value & active, dr::select(active, pdf, 0.f) };
    }

    /** \brief Compute the perturbation due to the normal map relative to \c si.sh_frame,
     * as well as the full \c sh_frame of the perturbation in the world coordinate system.
     */
    std::pair<Frame3f, Frame3f> frame(const SurfaceInteraction3f &si, Mask active) const {
        Normal3f n = dr::fmadd(m_normalmap->eval_3(si, active), 2, -1.f);

        if (m_flip_invalid_normals) {
            // Ensure that shading normals are always facing the incident direction.
            Mask flip = Frame3f::cos_theta(si.wi) * dr::dot(si.wi, n) <= 0.0f;
            n[flip] = Normal3f(-n.x(), -n.y(), n.z());
        }

        Frame3f frame_wrt_si;
        frame_wrt_si.n = dr::normalize(n);
        frame_wrt_si.s = dr::normalize(dr::fnmadd(frame_wrt_si.n, frame_wrt_si.n.x(), Vector3f(1, 0, 0)));
        frame_wrt_si.t = dr::cross(frame_wrt_si.n, frame_wrt_si.s);

        Frame3f frame_wrt_world;
        frame_wrt_world.n = si.to_world(frame_wrt_si.n);
        frame_wrt_world.s = si.to_world(frame_wrt_si.s);
        frame_wrt_world.t = si.to_world(frame_wrt_si.t);

        return { frame_wrt_si, frame_wrt_world };
    }

    Frame3f sh_frame(const SurfaceInteraction3f &si, Mask active) const override {
        return frame(si, active).second;
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

    MI_DECLARE_CLASS(NormalMap)
protected:
    ref<Base> m_nested_bsdf;
    ref<Texture> m_normalmap;

    bool m_flip_invalid_normals;
    bool m_use_shadowing_function;

    MI_TRAVERSE_CB(Base, m_nested_bsdf, m_normalmap)
};

MI_EXPORT_PLUGIN(NormalMap);
NAMESPACE_END(mitsuba)
