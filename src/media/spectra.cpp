#include <mitsuba/core/frame.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/volume.h>

#include <mitsuba/render/volumegrid.h>
#include <mitsuba/core/fresolver.h>
#include <drjit/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _medium-heterogeneous:

Spectra medium (:monosp:`specmedium`)
-----------------------------------------------

.. list-table::
 :widths: 20 15 65
 :header-rows: 1
 :class: paramstable

 * - Parameter
   - Type
   - Description

 * - albedo_[name]
   - |float| or |spectrum|
   - Single-scattering albedo of the medium (Default: 0.75). It is important to follow
     the name convention.

 * - sigmat_[name]
   - |spectrum|
   - Extinction coefficient in inverse scene units (Default: 1). It is important to
     follow the name convention.

 * - proportions
   - |volume|
   - Volume describing the proportions of each element in the mixture, with as many
     channels as elements

 * - scale
   - |float|
   - Optional scale factor that will be applied to the extinction parameter.
     It is provided for convenience when accommodating data based on different
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

This plugin provides a flexible spectral heterogeneous medium implementation that represents
a mixture of several spectrums. It does it efficiently by keeping in memory only one copy of the
spectral information, and combining them as follow:

.. math::
    \mu_t(x, \lambda) = \sum_i^N p^{(i)}(x) \, \mu_t^{(i)}(\lambda)

being :math:`N` the number of elements in the mixture, for the element :math:`i` its proportion
is :math:`p^{(i)}(x)`, and its spectral extinction coefficient is :math:`\mu_t^{(i)}(\lambda)`.

The following xml snippet describes a heterogenous spectral medium composed of a mixture of two
only-absorbing spectra, and a isotrophic phase function. Note how both extinction and
albedo parameters should be in the same alphabetical order to be match during the construction
of the plugin, and the proportions volume should contain the same number the channel as elements
in the mixture.

.. code-block:: xml

    <medium type="specmedium" id="media">
        <volume type="gridvolume" name="proportions">
            <string name="filename" value="proportions.vol"/>
        </volume>
        <spectrum name="sigmat_elem1" filename="spectra_absorption1.spd"/>
        <spectrum name="sigmat_elem2" filename="spectra_absorption2.spd"/>
        <spectrum name="albedo_elem1" value="0.0"/>
        <spectrum name="albedo_elem2" value="0.0"/>
    </medium>

*/
template <typename Float, typename Spectrum>
class SpectraMedium final : public Medium<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction,
                    m_phase_function)
    MTS_IMPORT_TYPES(Scene, Sampler, Texture, Volume, VolumeGrid)

    SpectraMedium(const Properties &props) : Base(props) {
        if constexpr (!is_spectral_v<Spectrum>)
            Log(Error, "This media can only be used in Mitsuba variants that "
                       "perform a spectral simulation.");

        m_is_homogeneous = false;
        m_proportions = props.volume<Volume>("proportions");

        if (m_proportions->max() == -999.0f)
            Log(Error, "You need to define a \"proportions\" volume");

        m_scale = props.get<ScalarFloat>("scale", 1.0f);
        m_has_spectral_extinction = true;
        std::vector<ScalarFloat> max_proportions_grid = m_proportions->max_generic();
        ScalarFloat max_density = 0.f;

        uint32_t index = 0;
        // Load all spectrum data
        for (auto &[name, obj] : props.objects(false)) {
            auto srf = dynamic_cast<Texture *>(obj.get());
            if (srf != nullptr) {
                if (name.rfind("sigmat_", 0) == 0) {
                    m_spectra_sigmat.push_back(srf);
                    m_names_sigmat.push_back(name);
                    props.mark_queried(name);
                    max_density += max_proportions_grid[index] * srf->max();
                    index++;
                } else if (name.rfind("albedo_", 0) == 0) {
                    m_spectra_albedo.push_back(srf);
                    m_names_albedo.push_back(name);
                    props.mark_queried(name);
                } else {
                    Log(Error, "Name %s is not valid. Should start with \"sigmat_\" or \"albedo_\".");
                }
            }
        }

        m_max_density = dr::opaque<Float>(m_scale * max_density);

        dr::set_attr(this, "is_homogeneous", m_is_homogeneous);
        dr::set_attr(this, "has_spectral_extinction", m_has_spectral_extinction);
    }

    UnpolarizedSpectrum
    get_combined_extinction(const MediumInteraction3f & /* mi */,
                            Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        return m_max_density;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        auto proportions = m_proportions->eval_generic_1(mi, active);

        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.wavelengths = mi.wavelengths;

        UnpolarizedSpectrum sigmat(0.0), albedo(0.0);
        for (size_t i=0; i<proportions.size(); ++i) {
            sigmat += proportions[i] * m_spectra_sigmat[i]->eval(si, active);
            albedo += proportions[i] * m_spectra_albedo[i]->eval(si, active);
        }
        sigmat *= m_scale;

        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigmat *= m_phase_function->projected_area(mi, active);

        UnpolarizedSpectrum sigmas = sigmat * albedo;
        UnpolarizedSpectrum sigman = m_max_density - sigmat;
        return { sigmas, sigman, sigmat };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_proportions->bbox().ray_intersect(ray);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        ScalarFloat max_density = 0.f;
        for(size_t i=0; i<m_spectra_sigmat.size(); ++i) {
            max_density += m_spectra_sigmat[i]->max();
        }
        m_max_density = dr::opaque<Float>(m_scale * max_density);
    }

    void traverse(TraversalCallback *callback) override {
        for (size_t i=0; i<m_names_sigmat.size(); ++i) {
            callback->put_object(m_names_sigmat[i], m_spectra_sigmat[i].get());
            callback->put_object(m_names_albedo[i], m_spectra_albedo[i].get());
        }
        callback->put_object("proportions", m_proportions.get());
        Base::traverse(callback);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Spectra Medium[" << std::endl
            // << "  albedo  = " << string::indent(m_albedo) << std::endl
            << "  proportions = " << string::indent(m_proportions) << "," << std::endl
            << "  scale   = " << string::indent(m_scale) << "," << std::endl
            << "  max_density = " << string::indent(m_max_density) << "," << std::endl;

        oss << "  sigmat = [";
        for (size_t i=0; i<m_names_sigmat.size()-1; ++i) {
            oss << m_names_sigmat[i] << ",";
        }
        oss << m_names_sigmat[m_names_sigmat.size()-1] << " ]," << std::endl;

        oss << "  albedo = [";
        for (size_t i=0; i<m_names_albedo.size()-1; ++i) {
            oss << m_names_albedo[i] << ",";
        }
        oss << m_names_albedo[m_names_albedo.size()-1] << " ]," << std::endl;

        oss << "  phase_function = [" << std::endl << string::indent(m_phase_function) << "]";

        oss << "  ],";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFloat m_scale;

    // Save spectrum information
    std::vector<ref<Texture>> m_spectra_sigmat;
    std::vector<ref<Texture>> m_spectra_albedo;
    std::vector<std::string> m_names_sigmat;
    std::vector<std::string> m_names_albedo;
    // Save proportions volume
    ref<Volume> m_proportions;
    // Save precomputed medium
    std::vector<UnpolarizedSpectrum> m_data;

    Float m_max_density;
};

MTS_IMPLEMENT_CLASS_VARIANT(SpectraMedium, Medium)
MTS_EXPORT_PLUGIN(SpectraMedium, "Spectra medium")
NAMESPACE_END(mitsuba)
