#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>
// #include <enoki/stl.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-diffuse:

Normal map BSDF (:monosp:`normalmap`)
-------------------------------------

TODO: documentation

*/
template <typename Float, typename Spectrum>
class NormalMap final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    NormalMap(const Properties &props) : Base(props) {
        for (auto &kv : props.objects()) {
            auto bsdf = dynamic_cast<Base *>(kv.second.get());

            if (bsdf) {
                if (m_nested_bsdf)
                    Throw("Only a single BSDF child object can be specified.");
                m_nested_bsdf = bsdf;
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

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        // Sample nested BSDF with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi = perturbed_si.to_local(si.to_world(si.wi));
        auto [bs, weight] = m_nested_bsdf->sample(ctx, perturbed_si,
                                                  sample1, sample2, active);
        active &= any(neq(weight, 0.f));
        if (none(active))
            return { bs, 0.f };

        // Transform sampled 'wo' back to original frame and check orientation
        Vector3f perturbed_wo = si.to_local(perturbed_si.to_world(bs.wo));
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
        perturbed_si.wi       = perturbed_si.to_local(si.to_world(si.wi));
        Vector3f perturbed_wo = perturbed_si.to_local(si.to_world(wo));

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        return m_nested_bsdf->eval(ctx, perturbed_si, perturbed_wo, active);
    }


    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        // Evaluate nested BSDF with perturbed shading frame
        SurfaceInteraction3f perturbed_si(si);
        perturbed_si.sh_frame = frame(si, active);
        perturbed_si.wi       = perturbed_si.to_local(si.to_world(si.wi));
        Vector3f perturbed_wo = perturbed_si.to_local(si.to_world(wo));

        active &= Frame3f::cos_theta(wo) *
                  Frame3f::cos_theta(perturbed_wo) > 0.f;

        return m_nested_bsdf->pdf(ctx, perturbed_si, perturbed_wo, active);
    }

    Frame3f frame(const SurfaceInteraction3f &si, Mask active) const {
        Normal3f n = fmadd(m_normalmap->eval_3(si, active), 2, -1.f);

        Frame3f result;
        result.n = normalize(n);
        result.s = normalize(fnmadd(result.n, dot(result.n, si.dp_du), si.dp_du));
        result.t = cross(result.n, result.s);
        return result;
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

    MTS_DECLARE_CLASS()
protected:
    ref<Base> m_nested_bsdf;
    ref<Texture> m_normalmap;
};

MTS_IMPLEMENT_CLASS_VARIANT(NormalMap, BSDF)
MTS_EXPORT_PLUGIN(NormalMap, "Normal map material adapter");
NAMESPACE_END(mitsuba)
