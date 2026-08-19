#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/texture.h>

#include "perturbed_bsdf.h"

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
class NormalMap final
    : public PerturbedBSDF<NormalMap<Float, Spectrum>, Float, Spectrum> {
public:
    using Base = PerturbedBSDF<NormalMap<Float, Spectrum>, Float, Spectrum>;
    MI_USING_MEMBERS(m_nested_bsdf)
    MI_IMPORT_TYPES(Field)

    NormalMap(const Properties &props)
        // Tangent-space normals need the mesh to supply a UV-oriented tangent
        : Base(props, (uint32_t) BSDFFlags::NormalMapped) {
        m_normalmap = props.get_surface_field<Field>("normalmap");
        FieldValueType type = m_normalmap->out_type();
        uint32_t dim = m_normalmap->out_dim();
        bool rgb_spectrum = is_rgb_v<Spectrum> &&
                            type == FieldValueType::Spectrum && dim == 3;
        if (!((type == FieldValueType::Color3 ||
               type == FieldValueType::Array3) && dim == 3) &&
            !rgb_spectrum)
            Throw("NormalMap: parameter \"normalmap\" must be a "
                  "three-channel surface field (Color3[3], Array3[3], or "
                  "RGB Spectrum[3]), got %s[%u].",
                  field_value_type_name(type), dim);
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("normalmap", m_normalmap, ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    Normal3f perturbation(const SurfaceInteraction3f &si, Mask active) const {
        return Normal3f(dr::fmadd(m_normalmap->eval_3(si, active), 2, -1.f));
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
    ref<Field> m_normalmap;

    MI_TRAVERSE_CB(Base, m_normalmap)
};

MI_EXPORT_PLUGIN(NormalMap);
NAMESPACE_END(mitsuba)
