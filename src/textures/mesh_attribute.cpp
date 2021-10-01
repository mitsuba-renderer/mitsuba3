#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-meshattribute:

Mesh attribute texture (:monosp:`mesh_attribute`)
-------------------------------------------------

.. pluginparameters::

 * - name
   - |string|
   - Name of the attribute to evaluate. It should always start with ``"vertex_"`` or ``"face_"``.
 * - scale
   - |float|
   - Scaling factor applied to the interpolated attribute value during evalutation.
     (Default: 1.0)

This plugin provides a simple mechanism to expose Mesh attributes (e.g. vertex color)
as a texture.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/texture_mesh_attribute_vertex.jpg
   :caption: Bunny with random vertex color (using barycentric interpolation).
.. subfigure:: ../../resources/data/docs/images/render/texture_mesh_attribute_face.jpg
   :caption: Bunny with random face color.
.. subfigend::
    :label: fig-texture-mesh-attribute

The following XML snippet describes a mesh with diffuse material,
whose reflectance is specified using the ``vertex_color`` attribute of that mesh:

.. code-block:: xml

    <shape type="ply">
        <string name="filename" value="my_mesh_with_vertex_color_attr.ply"/>

        <bsdf type="diffuse">
            <texture type="mesh_attribute" name="reflectance">
                <string name="name" value="vertex_color"/>
            </texture>
        </bsdf>
    </shape>

.. note::

    For spectral variants of the renderer (e.g. ``scalar_spectral``), when a mesh attribute name
    contains the string ``"color"``, the tri-stimulus RGB values will be converted to ``rgb2spec``
    model coefficients automatically.

 */

template <typename Float, typename Spectrum>
class MeshAttribute final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    MeshAttribute(const Properties &props)
    : Texture(props) {
        m_name = props.string("name");
        if (m_name.find("vertex_") == std::string::npos && m_name.find("face_") == std::string::npos)
            Throw("Invalid mesh attribute name: must be start with either \"vertex_\" or \"face_\" but was \"%s\".", m_name.c_str());

        m_scale = props.float_("scale", 1.f);
    }

    const std::string& name() const { return m_name; }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return si.shape->eval_attribute(m_name, si, active) * m_scale;
    }

    Float eval_1(const SurfaceInteraction3f &si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return si.shape->eval_attribute_1(m_name, si, active) * m_scale;
    }

    Color3f eval_3(const SurfaceInteraction3f &si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return si.shape->eval_attribute_3(m_name, si, active) * m_scale;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("scale", m_scale);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MeshAttribute[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  scale = \"" << m_scale << "\"" << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    std::string m_name;
    float m_scale;
};

MTS_IMPLEMENT_CLASS_VARIANT(MeshAttribute, Texture)
MTS_EXPORT_PLUGIN(MeshAttribute, "Mesh attribute")

NAMESPACE_END(mitsuba)
