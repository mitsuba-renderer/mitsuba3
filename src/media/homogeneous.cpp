#include <mitsuba/core/frame.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)


/**!

.. _medium-homogeneous:

Homogeneous medium (:monosp:`homogeneous`)
-----------------------------------------------

.. list-table::
 :widths: 20 15 65
 :header-rows: 1
 :class: paramstable

 * - Parameter
   - Type
   - Description
 * - albedo
   - |float|, |spectrum| or |volume|
   - Single-scattering albedo of the medium (Default: 0.75).

 * - sigma_t
   - |float| or |spectrum|
   - Extinction coefficient in inverse scene units (Default: 1).

 * - scale
   - |float|
   - Optional scale factor that will be applied to the extinction parameter.
     It is provided for convenience when accomodating data based on different
     units, or to simply tweak the density of the medium. (Default: 1)

 * - sample_emitters
   - |bool|
   - Flag to specify whether shadow rays should be cast from inside the volume (Default: |true|)
     If the medium is enclosed in a :ref:`dielectric <bsdf-dielectric>` boundary,
     shadow rays are ineffective and turning them off will significantly reduce
     render time. This can reduce render time up to 50% when rendering objects
     with subsurface scattering.

 * - (Nested plugin)
   - |phase|
   - A nested phase function that describes the directional scattering properties of
     the medium. When none is specified, the renderer will automatically use an instance of
     isotropic.

This class implements a homogeneous participating medium with support for arbitrary
phase functions. This medium can be used to model effects such as fog or subsurface scattering.

The medium is parametrized by the single scattering albedo and the extinction coefficient
:math:`\sigma_t`. The extinction coefficient should be provided in inverse scene units.
For instance, when a world-space distance of 1 unit corresponds to a meter, the
extinction coefficent should have units of inverse meters. For convenience,
the scale parameter can be used to correct the units. For instance, when the scene is in
meters and the coefficients are in inverse millimeters, set scale to 1000.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/medium_homogeneous_sss.jpg
   :caption: Homogeneous medium with constant albedo
.. subfigure:: ../../resources/data/docs/images/render/medium_homogeneous_sss_textured.jpg
   :caption: Homogeneous medium with spatially varying albedo
.. subfigend::
   :label: fig-homogeneous


The homogeneous medium assumes the extinction coefficient to be constant throughout the medium.
However, it supports the use of a spatially varying albedo.

.. code-block:: xml
    :name: lst-homogeneous

    <medium id="myMedium" type="homogeneous">
        <rgb name="albedo" value="0.99, 0.9, 0.96"/>
        <float name="sigma_t" value="5"/>

        <!-- The extinction is also allowed to be spectrally varying
             Since RGB values have to be in the [0, 1]
            <rgb name="sigma_t" value="0.5, 0.25, 0.8"/>
        -->

        <!-- A homogeneous medium needs to have a constant extinction,
            but can have a spatially varying albedo:

            <volume name="albedo" type="gridvolume">
                <string name="filename" value="albedo.vol"/>
            </volume>
        -->

        <phase type="hg">
            <float name="g" value="0.7"/>
        </phase>
    </medium>

*/

template <typename Float, typename Spectrum>
class HomogeneousMedium final : public Medium<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction, m_phase_function)
    MTS_IMPORT_TYPES(Scene, Sampler, Texture, Volume)

    HomogeneousMedium(const Properties &props) : Base(props) {
        m_is_homogeneous = true;
        m_albedo = props.volume<Volume>("albedo", 0.75f);
        m_sigmat = props.volume<Volume>("sigma_t", 1.f);

        m_scale = props.float_("scale", 1.0f);
        m_has_spectral_extinction = props.bool_("has_spectral_extinction", true);

        ek::set_attr(this, "is_homogeneous", m_is_homogeneous);
        ek::set_attr(this, "has_spectral_extinction", m_has_spectral_extinction);
    }

    MTS_INLINE auto eval_sigmat(const MediumInteraction3f &mi, Mask active) const {
        auto sigmat = m_sigmat->eval(mi) * m_scale;
        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigmat *= m_phase_function->projected_area(mi, active);
        return sigmat;
    }

    UnpolarizedSpectrum
    get_combined_extinction(const MediumInteraction3f &mi,
                            Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        return eval_sigmat(mi, active) & active;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        auto sigmat                = eval_sigmat(mi, active);
        auto sigmas                = sigmat * m_albedo->eval(mi, active);
        UnpolarizedSpectrum sigman = 0.f;

        return { sigmas & active, sigman, sigmat & active };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f & /* ray */) const override {
        return { true, 0.f, ek::Infinity<Float> };
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("scale", m_scale);
        callback->put_object("albedo", m_albedo.get());
        callback->put_object("sigma_t", m_sigmat.get());
        Base::traverse(callback);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HomogeneousMedium[" << std::endl
            << "  albedo  = " << string::indent(m_albedo) << std::endl
            << "  sigma_t = " << string::indent(m_sigmat) << std::endl
            << "  scale   = " << string::indent(m_scale)  << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ref<Volume> m_sigmat, m_albedo;
    ScalarFloat m_scale;
};

MTS_IMPLEMENT_CLASS_VARIANT(HomogeneousMedium, Medium)
MTS_EXPORT_PLUGIN(HomogeneousMedium, "Homogeneous Medium")
NAMESPACE_END(mitsuba)
