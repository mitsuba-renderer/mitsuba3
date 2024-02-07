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

.. pluginparameters::

 * - S
   - |volume|
   - A volume containing the SGGX parameters. The phase function is parametrized
     by six values :math:`S_{xx}`, :math:`S_{yy}`, :math:`S_{zz}`, :math:`S_{xy}`, :math:`S_{xz}` and
     :math:`S_{yz}` (see below for their meaning). The parameters can either be specified as a
     :ref:`constvolume <volume-constvolume>` with six values or as a :ref:`gridvolume <volume-gridvolume>`
     with six channels.
   - |exposed|, |differentiable|

This plugin implements the SGGX phase function :cite:`Heitz2015SGGX`.
The SGGX phase function is an anisotropic microflake phase function :cite:`Jakob10`.
This phase function can be useful to model fibers or surface-like structures using volume rendering.
The SGGX distribution is the distribution of normals (NDF) of a 3D ellipsoid.
It is parametrized by a symmetric, positive definite matrix :math:`S`.

Due to it's symmetry, the matrix :math:`S` is fully specified by providing the entries
:math:`S_{xx}`, :math:`S_{yy}`, :math:`S_{zz}`, :math:`S_{xy}`, :math:`S_{xz}` and :math:`S_{yz}`.
It is the responsibility of the user to ensure that these parameters describe a valid positive definite matrix.

.. tabs::
    .. code-tab:: xml

        <phase type='sggx'>
            <volume type="gridvolume" name="S">
                <string name="filename" value="volume.vol"/>
            </volume>
        </phase>

    .. code-tab:: python

        'type': 'sggx',
        'S': {
            'type': 'gridvolume',
            'filename': 'volume.vol'
        }

*/
template <typename Float, typename Spectrum>
class SGGXPhaseFunction final : public PhaseFunction<Float, Spectrum> {

public:
    MI_IMPORT_BASE(PhaseFunction, m_flags)
    MI_IMPORT_TYPES(PhaseFunctionContext, Volume)

    SGGXPhaseFunction(const Properties &props) : Base(props) {
        // m_diffuse    = props.get<bool>("diffuse", false);
        m_ndf_params = props.volume<Volume>("S");
        m_flags =
            PhaseFunctionFlags::Anisotropic | PhaseFunctionFlags::Microflake;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("S", m_ndf_params.get(), +ParamFlags::Differentiable);
    }

    MI_INLINE
    dr::Array<Float, 6> eval_ndf_params(const MediumInteraction3f &mi,
                                        Mask active) const {
        return m_ndf_params->eval_6(mi, active);
    }

    std::tuple<Vector3f, Spectrum, Float> sample(const PhaseFunctionContext & /* ctx */,
                                                 const MediumInteraction3f &mi,
                                                 const Float /* sample1 */,
                                                 const Point2f &sample2,
                                                 Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionSample, active);
        auto s         = eval_ndf_params(mi, active);
        auto sampled_n = sggx_sample(mi.sh_frame, sample2, s);

        // The diffuse variant of the SGGX is currently not supported and
        // requires some changes to the phase function interface to work in
        // GPU/LLVM modes
        /* if (m_diffuse) {
           Frame3f frame(sampled_n);
           auto wo =
           warp::square_to_cosine_hemisphere(ctx.sampler->next_2d(active));
           Float pdf = warp::square_to_cosine_hemisphere_pdf(wo);
           wo = frame.to_world(wo);
           return {wo, pdf};
           } else { */
        Float pdf = 0.25f * sggx_pdf(Vector3f(sampled_n), s) /
                    sggx_projected_area(mi.wi, s);
        Vector3f wo = dr::normalize(reflect(mi.wi, sampled_n));
        return { wo, 1.f, pdf };
        // }
    }

    std::pair<Spectrum, Float> eval_pdf(const PhaseFunctionContext & /* ctx */,
                                        const MediumInteraction3f &mi,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionEvaluate, active);
        auto s = eval_ndf_params(mi, active);
        /* if (m_diffuse) {
           auto sampled_n = sggx_sample(mi.sh_frame,
           ctx.sampler->next_2d(active), s); return dr::InvPi<Float> *
           dr::maximum(dr::dot(wo, sampled_n), 0.f); } else { */
        Float pdf = 0.25f * sggx_pdf(dr::normalize(wo + mi.wi), s) /
                    sggx_projected_area(mi.wi, s);
        // }

        return { pdf, pdf };
    }

    virtual Float projected_area(const MediumInteraction3f &mi,
                                 Mask active = true) const override {
        return sggx_projected_area(mi.wi, eval_ndf_params(mi, active));
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SGGXPhaseFunction[" << std::endl
            << "  ndf_params = " << m_ndf_params
            << std::endl
            // << "  diffuse = " << m_diffuse << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    // bool m_diffuse;
    ref<Volume> m_ndf_params;
};

MI_IMPLEMENT_CLASS_VARIANT(SGGXPhaseFunction, PhaseFunction)
MI_EXPORT_PLUGIN(SGGXPhaseFunction, "SGGX phase function")
NAMESPACE_END(mitsuba)
