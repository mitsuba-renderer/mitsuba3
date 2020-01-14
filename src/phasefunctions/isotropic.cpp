#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _phase-isotropic:

Isotropic phase function (:monosp:`isotropic`)
-----------------------------------------------

This phase function simulates completely uniform scattering,
where all directionality is lost after a single scattering
interaction. It does not have any parameters.

*/


template <typename Float, typename Spectrum>
class IsotropicPhaseFunction final : public PhaseFunction<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(PhaseFunction)
    MTS_IMPORT_TYPES()

    IsotropicPhaseFunction(const Properties & /* props */) {}

    Vector3f sample(const MediumInteraction3f & /* mi */, const Point2f &sample,
                               Mask /* active */) const override {
        return warp::square_to_uniform_sphere(sample);
    }

    Float eval(const Vector3f &wo, Mask /* active */) const override {
        return warp::square_to_uniform_sphere_pdf(wo);
    }

    std::string to_string() const override { return "IsotropicPhaseFunction[]"; }

    MTS_DECLARE_CLASS()
private:
};

MTS_IMPLEMENT_CLASS_VARIANT(IsotropicPhaseFunction, PhaseFunction)
MTS_EXPORT_PLUGIN(IsotropicPhaseFunction, "Isotropic phase function")
NAMESPACE_END(mitsuba)
