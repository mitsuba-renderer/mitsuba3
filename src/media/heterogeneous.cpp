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

NAMESPACE_BEGIN(mitsuba)

enum class ActivationType : uint32_t {
    None = 0,
    Exponential,
    SoftPlus,
    ReLU,
};
ActivationType activation_type_from_string(const std::string &str) {
    if (str == "none" || str == "")
        return ActivationType::None;
    if (str == "exponential" || str == "exp")
        return ActivationType::Exponential;
    if (str == "softplus" || str == "SoftPlus")
        return ActivationType::SoftPlus;
    if (str == "relu" || str == "ReLU")
        return ActivationType::ReLU;
    Throw("Unsupported activation type: '%s'", str);
}
std::string activation_type_to_string(ActivationType tp) {
    switch (tp) {
        case ActivationType::None: return "none";
        case ActivationType::Exponential: return "exponential";
        case ActivationType::SoftPlus: return "softplus";
        case ActivationType::ReLU: return "relu";

        default:
            Throw("Unsupported activation type: %s", (uint32_t) tp);
    };
}


/**!

.. _medium-heterogeneous:

Heterogeneous medium (:monosp:`heterogeneous`)
-----------------------------------------------

.. pluginparameters::

 * - albedo
   - |float|, |spectrum| or |volume|
   - Single-scattering albedo of the medium (Default: 0.75).
   - |exposed|, |differentiable|

 * - sigma_t
   - |float|, |spectrum| or |volume|
   - Extinction coefficient in inverse scene units (Default: 1).
   - |exposed|, |differentiable|

 * - scale
   - |float|
   - Optional scale factor that will be applied to the extinction parameter.
     It is provided for convenience when accommodating data based on different
     units, or to simply tweak the density of the medium. (Default: 1)
   - |exposed|

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


This plugin provides a flexible heterogeneous medium implementation, which acquires its data
from nested volume instances. These can be constant, use a procedural function, or fetch data from
disk, e.g. using a 3D grid.

The medium is parametrized by the single scattering albedo and the extinction coefficient
:math:`\sigma_t`. The extinction coefficient should be provided in inverse scene units.
For instance, when a world-space distance of 1 unit corresponds to a meter, the
extinction coefficient should have units of inverse meters. For convenience,
the scale parameter can be used to correct the units. For instance, when the scene is in
meters and the coefficients are in inverse millimeters, set scale to 1000.

Both the albedo and the extinction coefficient can either be constant or textured,
and both parameters are allowed to be spectrally varying.

.. tabs::
    .. code-tab:: xml
        :name: lst-heterogeneous

        <!-- Declare a heterogeneous participating medium named 'smoke' -->
        <medium type="heterogeneous" id="smoke">
            <!-- Acquire extinction values from an external data file -->
            <volume name="sigma_t" type="gridvolume">
                <string name="filename" value="frame_0150.vol"/>
            </volume>

            <!-- The albedo is constant and set to 0.9 -->
            <float name="albedo" value="0.9"/>

            <!-- Use an isotropic phase function -->
            <phase type="isotropic"/>

            <!-- Scale the density values as desired -->
            <float name="scale" value="200"/>
        </medium>

        <!-- Attach the index-matched medium to a shape in the scene -->
        <shape type="obj">
            <!-- Load an OBJ file, which contains a mesh version
                 of the axis-aligned box of the volume data file -->
            <string name="filename" value="bounds.obj"/>

            <!-- Reference the medium by ID -->
            <ref name="interior" id="smoke"/>
            <!-- If desired, this shape could also declare
                a BSDF to create an index-mismatched
                transition, e.g.
                <bsdf type="dielectric"/>
            -->
        </shape>

    .. code-tab:: python

        # Declare a heterogeneous participating medium named 'smoke'
        'smoke': {
            'type': 'heterogeneous',

            # Acquire extinction values from an external data file
            'sigma_t': {
                'type': 'gridvolume',
                'filename': 'frame_0150.vol'
            },

            # The albedo is constant and set to 0.9
            'albedo': 0.9,

            # Use an isotropic phase function
            'phase': {
                'type': 'isotropic'
            },

            # Scale the density values as desired
            'scale': 200
        },

        # Attach the index-matched medium to a shape in the scene
        'shape': {
            'type': 'obj',
            # Load an OBJ file, which contains a mesh version
            # of the axis-aligned box of the volume data file
            'filename': 'bounds.obj',

            # Reference the medium by ID
            'interior': 'smoke',
            # If desired, this shape could also declare
            # a BSDF to create an index-mismatched
            # transition, e.g.
            # 'bsdf': {
            #     'type': 'isotropic'
            # },
        }
*/
template <typename Float, typename Spectrum>
class HeterogeneousMedium final : public Medium<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction,
                   m_phase_function, m_majorant_grid,
                   m_majorant_resolution_factor, m_majorant_factor)
    MI_IMPORT_TYPES(Scene, Sampler, Texture, Volume)

    HeterogeneousMedium(const Properties &props) : Base(props) {
        m_is_homogeneous = false;
        m_albedo         = props.volume<Volume>("albedo", 0.75f);
        m_sigmat         = props.volume<Volume>("sigma_t", 1.f);
        m_emission       = props.volume<Volume>("emission", 0.f);

        m_density_activation = activation_type_from_string(
            props.string("density_activation", "none"));
        ScalarFloat default_param = dr::NaN<ScalarFloat>;
        if (m_density_activation == ActivationType::SoftPlus) {
            // Suggested value: 1e-6 for "coarse" model initialization,
            //                  1e-2 for "fine" model initialization.
            const ScalarFloat a = 1e-2f;
            // Approximate size of a voxel in world coordinates
            ScalarFloat voxel_size = dr::max(m_sigmat->voxel_size());
            default_param = dr::log(dr::pow(1.f - a, -1.f / voxel_size) - 1.f);
        }
        m_density_activation_parameter =
            props.get<float>("density_activation_parameter", default_param);
        if (m_density_activation != ActivationType::None) {
            Log(Info, "Heterogeneous medium using density activation '%s', parameter %s",
                activation_type_to_string(m_density_activation),
                m_density_activation_parameter);
        }

        ScalarFloat scale = props.get<float>("scale", 1.0f);
        m_has_spectral_extinction =
            props.get<bool>("has_spectral_extinction", true);
        m_scale = scale;

        update_majorant_supergrid();
        if (m_majorant_resolution_factor > 0) {
            Log(Info, "Using majorant supergrid with resolution %s", m_majorant_grid->resolution());
            m_max_density = dr::NaN<Float>;
        } else {
            const ScalarFloat vmax =
                density_activation(m_majorant_factor * scale * m_sigmat->max());
            m_max_density = dr::opaque<Float>(dr::maximum(1e-6f, vmax));
            Log(Info, "Heterogeneous medium will use majorant: %s (majorant factor: %s)",
                m_max_density, m_majorant_factor);
        }

        dr::set_attr(this, "is_homogeneous", m_is_homogeneous);
        dr::set_attr(this, "has_spectral_extinction", m_has_spectral_extinction);
    }

    void traverse(TraversalCallback *callback) override {
        // callback->put_parameter("scale", m_scale,          +ParamFlags::Differentiable);
        callback->put_object("albedo",   m_albedo.get(),   +ParamFlags::Differentiable);
        callback->put_object("sigma_t",  m_sigmat.get(),   +ParamFlags::Differentiable);
        callback->put_object("emission", m_emission.get(), +ParamFlags::Differentiable);
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        if (m_majorant_resolution_factor > 0) {
            // TODO: make this more configurable (could be slow)
            update_majorant_supergrid();
        } else {
            // TODO: make this really optional if we never need max_density
            const ScalarFloat vmax = density_activation(
                m_majorant_factor * m_scale.scalar() * m_sigmat->max());
            m_max_density = dr::opaque<Float>(dr::maximum(1e-6f, vmax));
            m_majorant_grid = nullptr;
            Log(Debug, "Heterogeneous medium majorant updated to: %s (majorant factor: %s)",
                m_max_density, m_majorant_factor);
        }
    }

    UnpolarizedSpectrum
    get_majorant(const MediumInteraction3f & mei,
                 Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        if (m_majorant_resolution_factor > 0)
            return m_majorant_grid->eval_1(mei, active);
        else
            return m_max_density;
    }

    UnpolarizedSpectrum get_albedo(const MediumInteraction3f &mei,
                                   Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        auto value = m_albedo->eval(mei, active);
        return value & active;
    }

    UnpolarizedSpectrum get_emission(const MediumInteraction3f &mei,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        auto value = m_emission->eval(mei, active);
        return value & active;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mei,
                                Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        auto sigmat =
            density_activation(m_scale.value() * m_sigmat->eval(mei, active));
        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigmat *= m_phase_function->projected_area(mei, active);

        auto sigmas = sigmat * m_albedo->eval(mei, active);

        UnpolarizedSpectrum local_majorant =
            (m_majorant_resolution_factor > 0)
                ? m_majorant_grid->eval_1(mei, active)
                : m_max_density;
        auto sigman = local_majorant - sigmat;

        return { sigmas, sigman, sigmat };
    }

    template <typename Value>
    Value density_activation(const Value &v) const {
        switch (m_density_activation) {
            case ActivationType::None:
                return v;
            case ActivationType::Exponential:
                return dr::exp(v);
            case ActivationType::SoftPlus:
                return dr::log(1.f +
                               dr::exp(v + m_density_activation_parameter));
            case ActivationType::ReLU:
                return dr::maximum(v, 0.f);
            default:
                Throw("Unsupported activation type: %s",
                      (uint32_t) m_density_activation);
        };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_sigmat->bbox().ray_intersect(ray);
    }

    void update_majorant_supergrid() {
        if (m_majorant_resolution_factor <= 0)
            return;

        // Build a majorant grid, with the scale factor baked-in for convenience
        TensorXf majorants = density_activation(
            m_sigmat->local_majorants(m_majorant_resolution_factor,
                                      m_majorant_factor * m_scale.scalar()));
        dr::eval(majorants);

        Properties props("gridvolume");
        props.set_string("filter_type", "nearest");
        props.set_transform("to_world", m_sigmat->world_transform());
        props.set_pointer("data", &majorants);
        m_majorant_grid = (Volume *) PluginManager::instance()
                              ->create_object<Volume>(props)
                              .get();
        Log(Info, "Majorant supergrid updated (resolution: %s)", m_majorant_grid->resolution());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HeterogeneousMedium[" << std::endl
            << "  albedo          = " << string::indent(m_albedo) << std::endl
            << "  sigma_t         = " << string::indent(m_sigmat) << std::endl
            << "  emission        = " << string::indent(m_emission) << std::endl
            << "  scale           = " << string::indent(m_scale) << std::endl
            << "  max_density     = " << string::indent(m_max_density) << std::endl
            << "  majorant_factor = " << string::indent(m_majorant_factor) << std::endl
            << "  majorant_resolution_factor   = " << string::indent(m_majorant_resolution_factor) << std::endl
            << "  majorant_grid                = " << string::indent(m_majorant_grid) << std::endl
            << "  density_activation           = " << activation_type_to_string(m_density_activation) << std::endl
            << "  density_activation_parameter = " << m_density_activation_parameter << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Volume> m_sigmat, m_albedo, m_emission;
    field<Float> m_scale;
    Float m_max_density;
    ActivationType m_density_activation;
    ScalarFloat m_density_activation_parameter;
};

MI_IMPLEMENT_CLASS_VARIANT(HeterogeneousMedium, Medium)
MI_EXPORT_PLUGIN(HeterogeneousMedium, "Heterogeneous Medium")
NAMESPACE_END(mitsuba)
