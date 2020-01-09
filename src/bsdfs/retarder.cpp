#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class LinearRetarder final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    LinearRetarder(const Properties &props) : Base(props) {
        m_theta = props.texture<Texture>("theta", 0.f);
        // As default, instantiate as a quarter-wave plate
        m_delta = props.texture<Texture>("delta", 90.f);

        m_flags = BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::Null;
        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /* sample1 */, const Point2f &/* sample2 */,
                                             Mask active) const override {
        ScopedPhase sp(ProfilerPhase::BSDFSample);

        BSDFSample3f bs = zero<BSDFSample3f>();
        bs.wo = -si.wi;
        bs.pdf = 1.f;
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::Null;
        bs.sampled_component = 0;

        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = deg_to_rad(m_theta->eval(si, active));

            // Query phase difference
            UnpolarizedSpectrum delta = deg_to_rad(m_delta->eval(si, active));

            // Approximate angle-of-incidence behaviour with a cosine falloff
            delta *= abs(Frame3f::cos_theta(si.wi));

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_retarder(delta);

            // Rotate optical element by specified angle
            M = mueller::rotated_element(theta, M);

            // Forward direction is always away from light source.
            Vector3f forward = ctx.mode == TransportMode::Radiance ? si.wi : -si.wi;

            // Rotate in/out basis of M s.t. it alignes with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));
            return { bs, M };
        } else {
            return { bs, 1.f };
        }
    }

    Spectrum eval(const BSDFContext &/* ctx */, const SurfaceInteraction3f &/* si */,
                  const Vector3f &/* wo */, Mask /* active */) const override {
        ScopedPhase sp(ProfilerPhase::BSDFEvaluate);
        return 0.f;
    }

    Float pdf(const BSDFContext &/* ctx */, const SurfaceInteraction3f &/* si */,
              const Vector3f &/* wo */, Mask /* active */) const override {
        ScopedPhase sp(ProfilerPhase::BSDFEvaluate);
        return 0.f;
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f &si, Mask active) const override {
        if constexpr (is_polarized_v<Spectrum>) {
            // Query rotation angle
            UnpolarizedSpectrum theta = deg_to_rad(m_theta->eval(si, active));

            // Query phase difference
            UnpolarizedSpectrum delta = deg_to_rad(m_delta->eval(si, active));

            // Approximate angle-of-incidence behaviour with a cosine falloff
            delta *= abs(Frame3f::cos_theta(si.wi));

            // Get standard Mueller matrix for a linear polarizer.
            Spectrum M = mueller::linear_retarder(delta);

            // Rotate optical element by specified angle
            M = mueller::rotated_element(theta, M);

            // Forward direction is always away from light source.
            Vector3f forward = si.wi;   // Note: when tracing Importance, this should be reversed.

            // Rotate in/out basis of M s.t. it alignes with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));
            return M;
        } else {
            return 1.f;
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("theta", m_theta.get());
        callback->put_object("delta", m_delta.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "LinearPolarizer[" << std::endl
            << "  theta = " << string::indent(m_theta) << std::endl
            << "  delta = " << string::indent(m_delta) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Texture> m_theta;
    ref<Texture> m_delta;
};

MTS_IMPLEMENT_CLASS_VARIANT(LinearRetarder, BSDF)
MTS_EXPORT_PLUGIN(LinearRetarder, "Linear retarder material")
NAMESPACE_END(mitsuba)