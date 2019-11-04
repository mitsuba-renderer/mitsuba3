#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/texture3d.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Constant3D final : public Texture3D<Float, Spectrum> {
public:
    MTS_REGISTER_CLASS(Constant3D, Texture3D)
    MTS_USING_BASE(Texture3D, m_world_to_local)
    MTS_IMPORT_TYPES(ContinuousSpectrum)

    explicit Constant3D(const Properties &props) : Base(props) {
        m_color = props.spectrum<Float, Spectrum>("color", 1.f);
    }

    std::vector<ref<Object>> children() override { return { m_color.get() }; }

    Spectrum eval(const Interaction3f &it, Mask active) const override {
        return eval_impl<false>(it, active);
    }

    std::pair<Spectrum, Vector3f> eval_gradient(const Interaction3f &it,
                                                Mask active) const override {
        return eval_impl<true>(it, active);
    }

    template <bool with_gradient>
    MTS_INLINE auto eval_impl(const Interaction3f &it, const Mask &active) const {
        Spectrum result = 0.f;

        auto p         = m_world_to_local * it.p;
        Mask inside    = active && all((p >= 0) && (p <= 1));
        result[inside] = m_color->eval(it.wavelengths, inside);

        if constexpr (with_gradient)
            return std::make_pair(result, zero<Vector3f>());
        else
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

protected:
    ref<ContinuousSpectrum> m_color;
};

MTS_IMPLEMENT_PLUGIN(Constant3D, Texture3D, "Constant 3D texture")
NAMESPACE_END(mitsuba)
