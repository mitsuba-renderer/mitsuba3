#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class OpenPBR final : public BSDF<Float, Spectrum> {

public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture, MicrofacetDistribution)

    OpenPBR(const Properties &props) : Base(props) {
        m_base_weight = props.get<Float>("base_weight", 1.f);
        m_base_color = props.get_texture<Texture>("base_color", 0.8f);
        m_base_metalness = props.get<Float>("base_metalness", 0.f);
        m_base_diffuse_roughness = props.get<Float>("base_diffuse_roughness", 0.f);

        m_specular_weight = props.get<Float>("specular_weight", 1.f);
        m_specular_color = props.get_texture<Texture>("specular_color", 0.1f);
        m_specular_roughness = props.get<Float>("specular_roughness", 0.3f);
        m_specular_roughness_anisotropy = props.get<Float>("specular_roughness_anisotropy", 0.f);
        m_specular_ior = props.get<Float>("specular_ior", 1.5f);

        m_transmission_weight = props.get<Float>("transmission_weight", 0.f);
        m_transmission_color = props.get_texture<Texture>("transmission_color", 1.f);
        m_transmission_depth = props.get<Float>("transmission_depth", 0.f);
        m_transmission_scatter = props.get_texture<Texture>("transmission_scatter", 0.f);
        m_transmission_scatter_anisotropy = props.get<Float>("transmission_scatter_anisotropy", 0.f);
        m_transmission_dispersion_scale = props.get<Float>("transmission_dispersion_scale", 0.f);
        m_transmission_dispersion_abbe_number = props.get<Float>("transmission_dispersion_abbe_number", 20.f);

        m_subsurface_weight = props.get<Float>("subsurface_weight", 0.f);
        m_subsurface_color = props.get_texture<Texture>("subsurface_color", 0.8f);
        m_subsurface_radius = props.get<Float>("subsurface_radius", 1.f);
        m_subsurface_radius_scale = props.get_texture<Texture>("subsurface_radius_scale", 1.f);
        m_subsurface_scatter_anisotropy = props.get<Float>("subsurface_scatter_anisotropy", 0.f);

        m_coat_weight = props.get<Float>("coat_weight", 0.f);
        m_coat_color = props.get_texture<Texture>("coat_color", 1.f);
        m_coat_roughness = props.get<Float>("coat_roughness", 0.f);
        m_coat_roughness_anisotropy = props.get<Float>("coat_roughness_anisotropy", 0.f);
        m_coat_ior = props.get<Float>("coat_ior", 1.6f);
        m_coat_darkening = props.get<Float>("coat_darkening", 1.f);

        m_fuzz_weight = props.get<Float>("fuzz_weight", 0.f);
        m_fuzz_color = props.get_texture<Texture>("fuzz_color", 1.f);
        m_fuzz_roughness = props.get<Float>("fuzz_roughness", .5f);

        m_emission_luminance = props.get<Float>("emission_luminance", 0.f);
        m_emission_color = props.get_texture<Texture>("emission_color", 1.f);

        m_thin_film_weight = props.get<Float>("thin_film_weight", 0.f);
        m_thin_film_thickness = props.get<Float>("thin_film_thickness", 0.5f);
        m_thin_film_ior = props.get<Float>("thin_film_ior", 1.4f);

        m_geometry_opacity = props.get<Float>("geometry_opacity", 1.f);
        m_geometry_thin_walled = props.get<bool>("geometry_thin_walled", false);
        m_geometry_normal = props.get<Vector3f>("geometry_normal", Vector3f(0.f, 0.f, 1.f));
        m_geometry_tangent = props.get<Vector3f>("geometry_tangent", Vector3f(1.f, 0.f, 0.f));
        m_geometry_coat_normal = props.get<Vector3f>("geometry_coat_normal", Vector3f(0.f, 0.f, 1.f));
        m_geometry_coat_tangent = props.get<Vector3f>("geometry_coat_tangent", Vector3f(1.f, 0.f, 0.f));

        initialize_lobes();
    }

    std::string to_string() const override {
        return "OpenPBR";
    };

    MI_DECLARE_CLASS(OpenPBR)
private:
    /// Parameters
    /// Base
    Float m_base_weight;
    ref<Texture> m_base_color;
    Float m_base_metalness;
    Float m_base_diffuse_roughness;

    /// Specular
    Float m_specular_weight;
    ref<Texture> m_specular_color;
    Float m_specular_roughness;
    Float m_specular_roughness_anisotropy;
    Float m_specular_ior;

    /// Transmission
    Float m_transmission_weight;
    ref<Texture> m_transmission_color;
    Float m_transmission_depth;
    ref<Texture> m_transmission_scatter;
    Float m_transmission_scatter_anisotropy;
    Float m_transmission_dispersion_scale;
    Float m_transmission_dispersion_abbe_number;

    /// Subsurface
    Float m_subsurface_weight;
    ref<Texture> m_subsurface_color;
    Float m_subsurface_radius;
    ref<Texture> m_subsurface_radius_scale;
    Float m_subsurface_scatter_anisotropy;

    /// Coat
    Float m_coat_weight;
    ref<Texture> m_coat_color;
    Float m_coat_roughness;
    Float m_coat_roughness_anisotropy;
    Float m_coat_ior;
    Float m_coat_darkening;

    /// Fuzz
    Float m_fuzz_weight;
    ref<Texture> m_fuzz_color;
    Float m_fuzz_roughness;

    /// Emission
    Float m_emission_luminance; // nits
    ref<Texture> m_emission_color;

    /// Thin-film
    Float m_thin_film_weight;
    Float m_thin_film_thickness; // micrometers
    Float m_thin_film_ior;

    /// Geometry
    Float m_geometry_opacity;
    bool m_geometry_thin_walled;
    Vector3f m_geometry_normal;
    Vector3f m_geometry_tangent;
    Vector3f m_geometry_coat_normal;
    Vector3f m_geometry_coat_tangent;

    void initialize_lobes() {
        m_components.push_back(BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide);

        if (m_coat_weight > 0.f) {
            uint32_t f = (BSDFFlags::GlossyReflection | BSDFFlags::FrontSide);
            if (m_coat_roughness_anisotropy > 0.f)
                f = f | BSDFFlags::Anisotropic;
            m_components.push_back(f);
        }

        if (m_fuzz_weight > 0.f)
            m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide);

        if (m_specular_weight > 0.f) {
            uint32_t f = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide;
            if (m_specular_roughness_anisotropy > 0.f)
                f = f | BSDFFlags::Anisotropic;
            m_components.push_back(f);
        }

        if (m_subsurface_weight > 0.f) {
            uint32_t f = BSDFFlags::Glossy | BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::NonSymmetric;
            if (m_subsurface_scatter_anisotropy > 0.f)
                f = f | BSDFFlags::Anisotropic;
            m_components.push_back(f);
        }

        if (m_transmission_weight > 0.f) {
            uint32_t f = BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide | BSDFFlags::BackSide;
            if (m_transmission_scatter_anisotropy > 0.f)
                f = f | BSDFFlags::Anisotropic;
            m_components.push_back(f);
        }
        if (m_thin_film_weight > 0.f)
            m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide);

        for (auto c : m_components)
            m_flags |= c;
    }
};

NAMESPACE_END(mitsuba)