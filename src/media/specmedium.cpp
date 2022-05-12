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

Spectral medium (:monosp:`specmedium`)
-----------------------------------------------

.. pluginparameters::

 * - albedo_[name]
   - |float| or |spectrum|
   - Single-scattering albedo of the medium. It is important to follow
     the name convention. This parameter will typically be specified several times with different
     values for ``_name``. Matching ``albedo`` and ``sigma_t`` should have the same ``[name]``.
   - |exposed|, |differentiable|

 * - sigmat_[name]
   - |spectrum|
   - Extinction coefficient in inverse scene units. It is important to
     follow the name convention. This parameter will typically be specified several times with
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
     render time. This improve performance by up to 2× when rendering objects
     with subsurface scattering.

 * - (Nested plugin)
   - |phase|
   - A nested phase function that describes the directional scattering properties of
     the medium. When none is specified, the renderer will automatically use an instance of
     an isotropic phase function.

A medium with high-resolution spectra can be extremely costly to store -- essentially, asymptotic
storage costs scale as :math:O(n^4). The implementation in this plugin uses a more efficient
representation that only stores the proportions of different compounds with known spectra
properties that are interpolated on the fly during medium queries.

For example, the spectral extinction :math:`\mu_t(x, \lambda)` for wavelength :math:`\lambda` at
position :math:`x` is evaluated as a sum over the compounds :math:`i=1,\ldots, N`
.. math::
    \mu_t(x, \lambda) = \sum_i^N p^{(i)}(x) \, \mu_t^{(i)}(\lambda),

where :math:`\mu_t^{(i)}(x, \lambda)` denotes the extinction of compound :math:`i` and
:math:`p^{(i)}(x)` denotes the proportion. A similar interpolation scheme is used for the albedo.

The association between medium channels storing mixture proportions and extinction/albedo spectra of
specific compounds is based on the alphanumeric order of their identifiers. For example, in a medium
with extinction parameters `sigmat_elem1` and `sigmat_elem2`, these will respectively be associated
with channels `0` and `1`. Also, note that the different extinction and albedo spectra will be related
if they share the suffix.

The following snippet describes a heterogenous spectral medium composed of a mixture of two
purely-absorbing spectra, and a isotropic phase function.

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
class SpectralMedium final : public Medium<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction,
                    m_phase_function)
    MI_IMPORT_TYPES(Scene, Sampler, Texture, Volume, VolumeGrid)

    using FloatStorage = DynamicBuffer<Float>;

    SpectralMedium(const Properties &props) : Base(props) {
        if constexpr (!is_spectral_v<Spectrum>)
            Log(Error, "This media can only be used in Mitsuba variants that "
                       "perform a spectral simulation.");

        m_is_homogeneous = false;
        m_proportions = props.volume<Volume>("proportions");

        m_scale = props.get<ScalarFloat>("scale", 1.0f);
        m_has_spectral_extinction = true;

        uint32_t index = 0;
        // Load all spectrum data
        for (auto &[name, obj] : props.objects(false)) {
            auto srf = dynamic_cast<Texture *>(obj.get());
            if (srf) {
                if (string::starts_with(name, "sigmat_")) {
                    m_spectra_sigma_t.push_back(srf);
                    m_names_sigma_t.push_back(name);
                    props.mark_queried(name);
                    index++;
                } else if (string::starts_with(name, "albedo_")) {
                    m_spectra_albedo.push_back(srf);
                    m_names_albedo.push_back(name);
                    props.mark_queried(name);
                } else {
                    Log(Error, "Spectrum passed to \"specmedium\" has an invalid name (\"%s\"). It must start with \"sigmat_\" or \"albedo_\".", name);
                }
            }
        }

        dr::set_attr(this, "is_homogeneous", m_is_homogeneous);
        dr::set_attr(this, "has_spectral_extinction", m_has_spectral_extinction);

        // Compute spectral majorant
        compute_spectral_majorant();
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
    get_majorant(const MediumInteraction3f &mi, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.wavelengths = mi.wavelengths;
        UnpolarizedSpectrum full_majorant = m_spectral_majorant->eval(si, active);
        return m_scale * full_majorant;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        size_t n = m_proportions->channel_count();
        Float *proportions = (Float *) alloca(sizeof(Float) * n);
        // call in-place default constructor in case `Float` is a non-POD type (e.g. JIT array)
        for (size_t i = 0; i < n; ++i)
            new (&proportions[i]) Float();

        m_proportions->eval_n(mi, proportions, active);

        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.wavelengths = mi.wavelengths;

        UnpolarizedSpectrum sigma_t(0.f), albedo(0.f);
        for (size_t i=0; i<n; ++i) {
            sigma_t += proportions[i] * m_spectra_sigma_t[i]->eval(si, active);
            albedo += proportions[i] * m_spectra_albedo[i]->eval(si, active);
        }
        sigma_t *= m_scale;

        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigma_t *= m_phase_function->projected_area(mi, active);

        // call destructor in case `Float` is a non-POD type (e.g. JIT array)
        for (size_t i = 0; i < n; ++i)
            proportions[i].~Float();

        UnpolarizedSpectrum sigma_s = sigma_t * albedo;
        UnpolarizedSpectrum sigma_n = get_majorant(mi, active) - sigma_t;
        return { sigma_s, sigma_n, sigma_t };
    }

    void compute_spectral_majorant() {
        ScalarFloat resolution = dr::Infinity<ScalarFloat>;
        ScalarVector2f range { dr::Infinity<ScalarFloat>, -dr::Infinity<ScalarFloat> };
        for (auto srf : m_spectra_sigma_t) {
            range.x() = dr::min(range.x(), srf->wavelength_range().x());
            range.y() = dr::max(range.y(), srf->wavelength_range().y());
            resolution = dr::min(resolution, srf->spectral_resolution());
        }

        size_t n_points = (size_t) dr::ceil((range.y() - range.x()) / resolution);
        FloatStorage spectral_majorant = dr::zero<FloatStorage>(n_points);
        Float majorant_wavelengths = dr::linspace<Float>(range.x(), range.y(), n_points);

        // Compute maximum proportion per grid channel
        std::vector<ScalarFloat> max_proportions_grid(m_proportions->channel_count());
        if (max_proportions_grid.size() == 0)
            Log(Error, "This plugins needs a volume that supports per-channel queries");
        m_proportions->max_per_channel(max_proportions_grid.data());

        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.wavelengths = majorant_wavelengths;

        uint32_t index = 0;
        for (auto srf : m_spectra_sigma_t) {
            UnpolarizedSpectrum values = max_proportions_grid[index] * srf->eval(si);
            spectral_majorant += values.x();
            index++;
        }

        // Conversion needed because Properties::Float is always double
        using DoubleStorage = dr::float64_array_t<FloatStorage>;
        DoubleStorage spectral_majorant_dbl = DoubleStorage(spectral_majorant);

        auto && storage = dr::migrate(spectral_majorant_dbl, AllocType::Host);
        if constexpr (dr::is_jit_array_v<Float>)
            dr::sync_thread();

        // Store majorant information
        auto props = Properties("regular");
        props.set_pointer("values", storage.data());
        props.set_long("size", n_points);
        props.set_float("lambda_min", (double) range.x());
        props.set_float("lambda_max", (double) range.y());
        m_spectral_majorant = PluginManager::instance()->create_object<Texture>(props);
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_proportions->bbox().ray_intersect(ray);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        compute_spectral_majorant();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Spectral Medium[" << std::endl
            << "  proportions = " << string::indent(m_proportions) << "," << std::endl
            << "  scale   = " << string::indent(m_scale) << "," << std::endl
            << "  max_density = " << string::indent(m_spectral_majorant->max()) << "," << std::endl;

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
    std::vector<ref<Texture>> m_spectra_sigma_t;
    std::vector<ref<Texture>> m_spectra_albedo;
    std::vector<std::string> m_names_sigma_t;
    std::vector<std::string> m_names_albedo;
    ref<Volume> m_proportions;
    ref<Texture> m_spectral_majorant;
};

MI_IMPLEMENT_CLASS_VARIANT(SpectralMedium, Medium)
MI_EXPORT_PLUGIN(SpectralMedium, "Spectral medium")
NAMESPACE_END(mitsuba)
