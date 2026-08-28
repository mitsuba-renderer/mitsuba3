#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/texture.h>

#include "perturbed_bsdf.h"

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
map. Note that we set the ``raw`` properties of the bump map ``bitmap`` object to
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
class BumpMap final
    : public PerturbedBSDF<BumpMap<Float, Spectrum>, Float, Spectrum> {
public:
    using Base = PerturbedBSDF<BumpMap<Float, Spectrum>, Float, Spectrum>;
    MI_USING_MEMBERS(m_nested_bsdf)
    MI_IMPORT_TYPES(Texture)

    BumpMap(const Properties &props) : Base(props) {
        for (auto &prop : props.objects()) {
            if (Texture *texture = prop.try_get<Texture>()) {
                if (m_nested_texture)
                    Throw("Only a single Texture child object can be specified.");
                m_nested_texture = texture;
            }
        }
        if (!m_nested_texture)
            Throw("Exactly one Texture child object must be specified.");

        m_scale = props.get<ScalarFloat>("scale", 1.f);

        // Probe the nested texture so that an unsuitable input is reported
        // when the scene is loaded rather than in the middle of a render
        try {
            SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
            m_nested_texture->eval_1_grad(si, false);
        } catch (const std::exception &e) {
            Throw("The nested texture cannot be evaluated as a scalar height "
                  "field.\n\n%s\n\nThe \"bumpmap\" plugin perturbs the "
                  "shading frame using the UV gradient of a height map. If the "
                  "texture instead encodes tangent-space normals in its RGB "
                  "channels, use the \"normalmap\" plugin.", e.what());
        }
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("nested_texture", m_nested_texture, ParamFlags::Differentiable | ParamFlags::Discontinuous);
        cb->put("scale",          m_scale,          ParamFlags::NonDifferentiable);
    }

    Normal3f perturbation(const SurfaceInteraction3f &si, Mask active) const {
        // Evaluate texture gradient
        Vector2f grad_uv = m_scale * m_nested_texture->eval_1_grad(si, active);

        // Compute perturbed differential geometry
        Vector3f dp_du = dr::fmadd(si.sh_frame.n, grad_uv.x() - dr::dot(si.sh_frame.n, si.dp_du), si.dp_du);
        Vector3f dp_dv = dr::fmadd(si.sh_frame.n, grad_uv.y() - dr::dot(si.sh_frame.n, si.dp_dv), si.dp_dv);

        // Bump-mapped shading normal, in world coordinates and unnormalized
        Vector3f cr = dr::cross(dp_du, dp_dv);

        // Flip if not aligned with geometric normal
        cr[dr::dot(si.n, cr) < .0f] *= -1.f;

        // Restate in the coordinate system of the original shading frame
        return Normal3f(si.to_local(cr));
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

    MI_DECLARE_CLASS(BumpMap)
protected:
    ScalarFloat m_scale;
    ref<Texture> m_nested_texture;

    MI_TRAVERSE_CB(Base, m_nested_texture)
};

MI_EXPORT_PLUGIN(BumpMap)
NAMESPACE_END(mitsuba)
