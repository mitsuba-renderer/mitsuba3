#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-null:

Null material (:monosp:`null`)
-------------------------------------------

This plugin models a completely invisible surface material.
Light will not interact with this BSDF in any way.

Internally, this is implemented as a forward-facing Dirac delta distribution.
Note that the standard :ref:`path tracer <integrator-path>` does not have a good sampling strategy to deal with this,
but the (:ref:`volumetric path tracer <integrator-volpath>`) does.

The main purpose of this material is to be used as the BSDF of a shape enclosing a participating medium.

 */

template <typename Float, typename Spectrum>
class Null final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    Null(const Properties &props) : Base(props) {
        m_components.push_back(BSDFFlags::Null | BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_flags = m_components.back();
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /*sample1*/,
                                             const Point2f & /*sample2*/,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);
        bool sample_transmission = ctx.is_enabled(BSDFFlags::Null, 0);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        Spectrum result(0.f);
        if (sample_transmission) {
            bs.wo                = -si.wi;
            bs.sampled_component = 0;
            bs.sampled_type      = UInt32(+BSDFFlags::Null);
            bs.eta               = 1.f;
            bs.pdf               = 1.f;

            /* In an ordinary BSDF we would use depolarizer<Spectrum>(1.f) here
               to construct a depolarizing Mueller matrix. However, the null
               BSDF should leave the polarization state unaffected, and hence
               this is one of the few places where it is safe to directly use a
               scalar (which will broadcast to the identity matrix in polarized
               rendering modes). */
            result               = 1.f;
        }

        return { bs, result };
    }

    Spectrum eval(const BSDFContext & /*ctx*/,
                  const SurfaceInteraction3f & /*si*/, const Vector3f & /*wo*/,
                  Mask /*active*/) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f & /*si*/,
                                    Mask /*active*/) const override {
        /* As above, we do not want the polarization state to change. So it is
           safe to return a scalar (which will broadcast to the identity
           matrix). */
        return 1.f;
    }

    std::string to_string() const override { return "Null[]"; }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(Null, BSDF)
MI_EXPORT_PLUGIN(Null, "Null material")
NAMESPACE_END(mitsuba)
