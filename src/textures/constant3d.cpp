#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/texture3d.h>

NAMESPACE_BEGIN(mitsuba)

class Constant3D final : public Texture3D {
public:
    explicit Constant3D(const Properties &props) : Texture3D(props) {
        m_color = props.spectrum("color", 1.f);
    }

    std::vector<ref<Object>> children() override { return { m_color.get() }; }

    template <typename Interaction,
              typename Value    = typename Interaction::Value,
              typename Spectrum = mitsuba::Spectrum<Value>>
    MTS_INLINE Spectrum eval_impl(const Interaction &it,
                                  const mask_t<Value> &active) const {
        using Mask      = mask_t<Value>;
        Spectrum result = 0.f;

        auto p         = m_world_to_local * it.p;
        Mask inside    = active && all((p >= 0) && (p <= 1));
        result[inside] = m_color->eval(it.wavelengths, inside);
        return result;
    }

    Float mean() const override { return m_color->mean(); }
    Float max() const override { NotImplementedError("max"); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Constant3D[" << std::endl
            << "  world_to_local = " << m_world_to_local << "," << std::endl
            << "  color = " << m_color << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_TEXTURE_3D()
    MTS_DECLARE_CLASS()

protected:
    ref<ContinuousSpectrum> m_color;
};

MTS_IMPLEMENT_CLASS(Constant3D, Texture3D)
MTS_EXPORT_PLUGIN(Constant3D, "Constant 3D texture")

NAMESPACE_END(mitsuba)
