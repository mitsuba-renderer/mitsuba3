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
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    Null(const Properties &props) : Base(props) {
        m_components.push_back(BSDFFlags::Null | BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_flags = m_components.back();
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                           Float /*sample1*/, const Point2f & /*sample2*/,
                                           Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);
        bool sample_transmission = ctx.is_enabled(BSDFFlags::Null, 0);
        BSDFSample3f bs = zero<BSDFSample3f>();
        Spectrum result(0.f);
        if (sample_transmission) {
            bs.wo                = -si.wi;
            bs.sampled_component = 0;
            bs.sampled_type      = UInt32(+BSDFFlags::Null);
            bs.eta               = 1.f;
            bs.pdf               = 1.f;
            result               = 1.f;
        }
        return { bs, unpolarized<Spectrum>(result) };
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
                  const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f & /*si*/,
                                    Mask /*active*/) const override {
        return unpolarized<Spectrum>(1.f);
    }

    std::string to_string() const override { return "Null[]"; }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(Null, BSDF)
MTS_EXPORT_PLUGIN(Null, "Null material")
NAMESPACE_END(mitsuba)
