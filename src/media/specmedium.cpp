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

.. _medium-specmedium:

Spectra medium (:monosp:`specmedium`)
-----------------------------------------------

.. pluginparameters::

 * - albedo_[name]
   - |float| or |spectrum|
   - Single-scattering albedo of the medium. It is important to follow
     the name convention. This parameter would be specified several times with different
     values for ``_name``. Matching ``albedo`` and ``sigma_t`` should have the same ``[name]``.
   - |exposed|, |differentiable|

 * - sigmat_[name]
   - |spectrum|
   - Extinction coefficient in inverse scene units. It is important to
     follow the name convention. This parameter would be specified several times with
     different values for ``_name``.
     Matching ``albedo`` and ``sigma_t`` should have the same ``[name]``.
   - |exposed|, |differentiable|

 * - proportions
   - |volume|
   - Volume describing the proportions of each element in the mixture, with as many
     channels as elements. Note that the order of its channels should match with the
     order of spectra properties (See below for more information on the order).
   - |exposed|, |differentiable|

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

A medium with high-resolution spectra can be extremely costly to store -- essentially, asymptotic
storage costs scale as to :math:O(n^4). The implementation in this plugin uses a more efficient
representation that only stores the proportions of different compounds with known spectra
properties that are interpolated on the fly during medium queries.

In the case of extinction, the medium works as follows (it is the same for albedo):
.. math::
    \mu_t(x, \lambda) = \sum_i^N p^{(i)}(x) \, \mu_t^{(i)}(\lambda)

being :math:`N` the number of elements in the mixture, for the element :math:`i` its proportion
is :math:`p^{(i)}(x)`, and its spectral extinction coefficient is :math:`\mu_t^{(i)}(\lambda)`.

Note that we need some order to relate the information of the different spectra (extinction/albedo)
with their corresponding ratios. Because of the way Mitsuba loads the information, it will order
the spectra by name (e.g. ``sigmat_elem1`` will have position ``0`` while ``sigmat_elem2`` will have
position ``1``). Therefore, the first channel of the volume representing the proportions will refer
to ``elem1``. Also, note that the different extinction and albedo spectra will be related if
they share the suffix.

The following snippet describes a heterogenous spectral medium composed of a mixture of two
only-absorbing spectra, and a isotrophic phase function.

.. tabs::
    .. code-tab::  xml

        <medium type="specmedium" id="media">
            <volume type="gridvolume" name="proportions">
                <string name="filename" value="proportions.vol"/>
                <boolean name="raw" value="true"/>
            </volume>
            <spectrum name="sigmat_elem1" filename="spectra_absorption1.spd"/>
            <spectrum name="sigmat_elem2" filename="spectra_absorption2.spd"/>
            <spectrum name="albedo_elem1" value="0.0"/>
            <spectrum name="albedo_elem2" value="0.0"/>
        </medium>

    .. code-tab:: python

        'type': 'specmedium',
        'volume': {
            'type': 'gridvolume',
            'filename': 'proportions.vol',
            'raw': True
        },
        'sigmat_elem1' {
            'type': 'spectrum',
            'filename': 'spectra_absorption1.spd'
        },
        'sigmat_elem2' {
            'type': 'spectrum',
            'filename': 'spectra_absorption2.spd'
        },
        'albedo_elem1' {
            'type': 'spectrum',
            'value': '0.0'
        },
        'albedo_elem2' {
            'type': 'spectrum',
            'value': '0.0'
        }

*/
template <typename Float, typename Spectrum>
class SpectraMedium final : public Medium<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction,
                    m_phase_function)
    MI_IMPORT_TYPES(Scene, Sampler, Texture, Volume, VolumeGrid)

    SpectraMedium(const Properties &props) : Base(props) {
        if constexpr (!is_spectral_v<Spectrum>)
            Log(Error, "This media can only be used in Mitsuba variants that "
                       "perform a spectral simulation.");

        m_is_homogeneous = false;
        m_proportions = props.volume<Volume>("proportions");

        m_scale = props.get<ScalarFloat>("scale", 1.0f);
        m_has_spectral_extinction = true;

        std::vector<ScalarFloat> max_proportions_grid(m_proportions->channel_count());
        if (max_proportions_grid.size() == 0)
            Log(Error, "This plugins needs a volume that supports per-channel queries");
        m_proportions->max_per_channel(max_proportions_grid.data());
        ScalarFloat max_density = 0.f;

        uint32_t index = 0;
        // Load all spectrum data
        for (auto &[name, obj] : props.objects(false)) {
            auto srf = dynamic_cast<Texture *>(obj.get());
            if (srf != nullptr) {
                if (name.rfind("sigmat_", 0) == 0) {
                    m_spectra_sigma_t.push_back(srf);
                    m_names_sigma_t.push_back(name);
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


    void traverse(TraversalCallback *callback) override {
        for (size_t i = 0; i < m_names_sigma_t.size(); ++i) {
            callback->put_object(m_names_sigma_t[i], m_spectra_sigma_t[i].get(), +ParamFlags::Differentiable);
            callback->put_object(m_names_albedo[i], m_spectra_albedo[i].get(),   +ParamFlags::Differentiable);
        }
        callback->put_object("proportions", m_proportions.get(),                 +ParamFlags::Differentiable);
        Base::traverse(callback);
    }

    UnpolarizedSpectrum
    get_combined_extinction(const MediumInteraction3f & /* mi */,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        return m_max_density;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        std::vector<Float> proportions(m_proportions->channel_count());
        m_proportions->eval_per_channel_1(mi, proportions.data(), active);

        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.wavelengths = mi.wavelengths;

        UnpolarizedSpectrum sigma_t(0.0), albedo(0.0);
        for (size_t i=0; i<proportions.size(); ++i) {
            sigma_t += proportions[i] * m_spectra_sigma_t[i]->eval(si, active);
            albedo += proportions[i] * m_spectra_albedo[i]->eval(si, active);
        }
        sigma_t *= m_scale;

        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigma_t *= m_phase_function->projected_area(mi, active);

        UnpolarizedSpectrum sigma_s = sigma_t * albedo;
        UnpolarizedSpectrum sigma_n = m_max_density - sigma_t;
        return { sigma_s, sigma_n, sigma_t };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_proportions->bbox().ray_intersect(ray);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        std::vector<ScalarFloat> max_proportions_grid(m_proportions->channel_count());
        m_proportions->max_per_channel(max_proportions_grid.data());

        ScalarFloat max_density = 0.f;
        for (size_t i = 0; i < max_proportions_grid.size(); ++i)
            max_density += max_proportions_grid[i] * m_spectra_sigma_t[i]->max();

        m_max_density = dr::opaque<Float>(m_scale * max_density);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Spectra Medium[" << std::endl
            << "  proportions = " << string::indent(m_proportions) << "," << std::endl
            << "  scale   = " << string::indent(m_scale) << "," << std::endl
            << "  max_density = " << string::indent(m_max_density) << "," << std::endl;

        oss << "  sigmat = [";
        for (size_t i=0; i<m_names_sigma_t.size()-1; ++i) {
            oss << m_names_sigma_t[i] << ",";
        }
        oss << m_names_sigma_t[m_names_sigma_t.size()-1] << " ]," << std::endl;

        oss << "  albedo = [";
        for (size_t i=0; i<m_names_albedo.size()-1; ++i) {
            oss << m_names_albedo[i] << ",";
        }
        oss << m_names_albedo[m_names_albedo.size()-1] << " ]," << std::endl;

        oss << "  phase_function = [" << std::endl << string::indent(m_phase_function) << "]";

        oss << "  ],";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ScalarFloat m_scale;
    Float m_max_density;
    std::vector<ref<Texture>> m_spectra_sigma_t;
    std::vector<ref<Texture>> m_spectra_albedo;
    std::vector<std::string> m_names_sigma_t;
    std::vector<std::string> m_names_albedo;
    ref<Volume> m_proportions;
};

MI_IMPLEMENT_CLASS_VARIANT(SpectraMedium, Medium)
MI_EXPORT_PLUGIN(SpectraMedium, "Spectra medium")
NAMESPACE_END(mitsuba)
