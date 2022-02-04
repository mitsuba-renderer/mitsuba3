#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/microflake.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _phase-sggx:

SGGX phase function (:monosp:`sggx`)
-------------------------------------


.. list-table::
 :widths: 20 15 65
 :header-rows: 1
 :class: paramstable

 * - Parameter
   - Type
   - Description

 * - S
   - |volume|
   - A volume containing the SGGX parameters. The phase function is parametrized
     by six values :math:`S_{xx}`, :math:`S_{yy}`, :math:`S_{zz}`, :math:`S_{xy}`, :math:`S_{xz}` and
     :math:`S_{yz}` (see below for their meaning). The parameters can either be specified as a
     :ref:`constvolume <volume-constvolume>` with six values or as a :ref:`gridvolume <volume-gridvolume>`
     with six channels.


This plugin implements the SGGX phase functuon :cite:`Heitz2015SGGX`.
The SGGX phase function is an anisotropic microflake phase function :cite:`Jakob10`.
This phase function can be useful to model fibers or surface-like structures using volume rendering.
The SGGX microflake distribution is parametrized by a symmetric, positive definite matrix :math:`S`.
This positive definite matrix describes the geometry of a 3D ellipsoid.
The microflake normals of the SGGX phase function correspond to normals of this ellisoid.

Due to it's symmetry, the matrix :math:`S` is fully specified by providing the entries
:math:`S_{xx}`, :math:`S_{yy}`, :math:`S_{zz}`, :math:`S_{xy}`, :math:`S_{xz}` and :math:`S_{yz}`.
It is the responsiblity of the user to ensure that these parameters describe a valid positive definite matrix.


*/
template <typename Float, typename Spectrum>
class SGGXPhaseFunction final : public PhaseFunction<Float, Spectrum> {

public:
    MTS_IMPORT_BASE(PhaseFunction, m_flags)
    MTS_IMPORT_TYPES(PhaseFunctionContext, Volume)

    SGGXPhaseFunction(const Properties &props) : Base(props) {
        // m_diffuse    = props.get<bool>("diffuse", false);
        m_ndf_params = props.volume<Volume>("S");
        m_flags = PhaseFunctionFlags::Anisotropic | PhaseFunctionFlags::Microflake;
        dr::set_attr(this, "flags", m_flags);
    }

    MTS_INLINE
    dr::Array<Float, 6> eval_ndf_params(const MediumInteraction3f &mi, Mask active) const {
        return m_ndf_params->eval_6(mi, active);
    }

    std::pair<Vector3f, Float> sample(const PhaseFunctionContext & /* ctx */,
                                      const MediumInteraction3f &mi,
                                      const Float /* sample1 */,
                                      const Point2f &sample2,
                                      Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionSample, active);
        auto s = eval_ndf_params(mi, active);
        auto sampled_n = sggx_sample_vndf(mi.sh_frame, sample2, s);

        // The diffuse variant of the SGGX is currently not supported and
        // requires some changes to the phase function interface to work in GPU/LLVM modes
        /* if (m_diffuse) {
            Frame3f frame(sampled_n);
            auto wo = warp::square_to_cosine_hemisphere(ctx.sampler->next_2d(active));
            Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);
            wo = frame.to_world(wo);
            return {wo, pdf};
        } else { */
        Float pdf = 0.25f * sggx_ndf_pdf(Vector3f(sampled_n), s) / sggx_projected_area(mi.wi, s);
        Vector3f wo = dr::normalize(reflect(mi.wi, sampled_n));
        return {wo, pdf};
        // }
    }

    Float eval(const PhaseFunctionContext & /* ctx */,
               const MediumInteraction3f &mi, const Vector3f &wo,
               Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionEvaluate, active);
        auto s = eval_ndf_params(mi, active);
        /* if (m_diffuse) {
            auto sampled_n = sggx_sample_vndf(mi.sh_frame, ctx.sampler->next_2d(active), s);
            return dr::InvPi<Float> * dr::max(dr::dot(wo, sampled_n), 0.f);
        } else { */
        return 0.25f * sggx_ndf_pdf(dr::normalize(wo + mi.wi), s) / sggx_projected_area(mi.wi, s);
        // }
    }

    virtual Float projected_area(const MediumInteraction3f &mi,
                                 Mask active = true) const override {
        return sggx_projected_area(mi.wi, eval_ndf_params(mi, active));
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("S", m_ndf_params);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SGGXPhaseFunction[" << std::endl
            << "  ndf_params = " << m_ndf_params << std::endl
            // << "  diffuse = " << m_diffuse << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    // bool m_diffuse;
    ref<Volume> m_ndf_params;
};

MTS_IMPLEMENT_CLASS_VARIANT(SGGXPhaseFunction, PhaseFunction)
MTS_EXPORT_PLUGIN(SGGXPhaseFunction, "SGGX phase function")
NAMESPACE_END(mitsuba)