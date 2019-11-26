#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class Constant3D final : public Texture3D<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(Constant3D, Texture3D)
    MTS_IMPORT_BASE(Texture3D, m_world_to_local)
    MTS_IMPORT_TYPES(Texture)

    explicit Constant3D(const Properties &props) : Base(props) {
        m_color = props.texture<Texture>("color", 1.f);
    }

    Spectrum eval(const Interaction3f &it, Mask active) const override {
        return eval_impl<false>(it, active);
    }

    Vector3f eval_3(const Interaction3f & /*it*/, Mask  /*active*/) const override {
        NotImplementedError("eval3");
    }

    Float eval_1(const Interaction3f & /*it*/, Mask  /*active*/) const override {
        NotImplementedError("eval1");
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
        //result[inside] = m_color->eval(it, inside);
        result[inside] = 0.f; // TODO

        if constexpr (with_gradient)
            return std::make_pair(result, zero<Vector3f>());
        else
            return result;
    }


    ScalarFloat max() const override { NotImplementedError("max"); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Constant3D[" << std::endl
            << "  world_to_local = " << m_world_to_local << "," << std::endl
            << "  color = " << m_color << std::endl
            << "]";
        return oss.str();
    }

protected:
    ref<Texture> m_color;
};

MTS_EXPORT_PLUGIN(Constant3D, "Constant 3D texture")
NAMESPACE_END(mitsuba)
