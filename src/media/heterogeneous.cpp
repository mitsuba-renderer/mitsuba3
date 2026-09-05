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


/**!

.. _medium-heterogeneous:

Heterogeneous medium (:monosp:`heterogeneous`)
-----------------------------------------------

.. pluginparameters::

 * - albedo
   - |float|, |spectrum| or |volume|
   - Single-scattering albedo of the medium (Default: 0.75). Mutually
     exclusive with :monosp:`sigma_s`.
   - |exposed|, |differentiable|

 * - sigma_s
   - |float|, |spectrum| or |volume|
   - Scattering coefficient, for scenes that store one instead of an albedo;
     the two are related by :math:`\sigma_s = \sigma_t\,\alpha` and are
     mutually exclusive. Whichever is given is the differentiable parameter,
     and the same :monosp:`scale` applies. See the note below before
     optimizing this one.
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

 * - majorant_resolution_factor
   - |int|
   - Delta tracking bounds the extinction by a *majorant*. Rather than one
     global bound, this medium builds a coarse grid of per-cell bounds -- a
     majorant supergrid -- by max-pooling :monosp:`sigma_t` with this factor
     along each axis, and traverses it with a DDA. Empty and thin regions are
     then crossed in far fewer null collisions. Set to 0 for a single global
     majorant. (Default: 8)

 * - majorant_factor
   - |float|
   - Safety factor applied on top of the per-cell maxima. (Default: 1.2)

 * - (Nested plugin)
   - |phase|
   - A nested phase function that describes the directional scattering properties of
     the medium. When none is specified, the renderer will automatically use an instance of
     isotropic.
   - |exposed|, |differentiable|


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
and both parameters are allowed to be spectrally varying. A scene that already
holds a scattering coefficient may give :monosp:`sigma_s` in place of the
albedo; the two are mutually exclusive.

.. note::

    Both render identically, but prefer the albedo when optimizing. With an
    independent :monosp:`sigma_s`, the derivative involves the ratio
    :math:`\sigma_s / \sigma_t` and becomes unstable as the extinction goes
    to zero. Optimizing :math:`\alpha = \sigma_s / \sigma_t` instead
    decouples that parameter from :math:`\sigma_t` and avoids the problem.

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
                    m_phase_function, has_majorant_grid, majorant_grid_eval,
                    update_majorant_grid)
    MI_IMPORT_TYPES(Scene, Sampler, Texture, Volume)

    HeterogeneousMedium(const Properties &props) : Base(props) {
        m_is_homogeneous = false;
        m_sigmat = props.get_volume<Volume>("sigma_t", 1.0f);

        /* Either the albedo or sigma_s, related by sigma_s = sigma_t * albedo.
           Whichever is given is the differentiable leaf. */
        bool has_albedo = props.has_property("albedo"),
             has_sigmas = props.has_property("sigma_s");
        if (has_albedo && has_sigmas)
            Throw("heterogeneous: 'albedo' and 'sigma_s' are two "
                  "parameterizations of the same quantity and are mutually "
                  "exclusive -- provide one or the other.");
        if ((has_albedo || has_sigmas) && !props.has_property("sigma_t"))
            Log(Warn, "heterogeneous: '%s' was given without a 'sigma_t' "
                      "volume, so the conversion between the two "
                      "parameterizations uses the default extinction of 1.",
                has_sigmas ? "sigma_s" : "albedo");
        if (has_sigmas)
            m_sigmas = props.get_volume<Volume>("sigma_s");
        else
            m_albedo = props.get_volume<Volume>("albedo", 0.75f);

        m_scale = props.get<ScalarFloat>("scale", 1.0f);
        m_has_spectral_extinction = props.get<bool>("has_spectral_extinction", true);

        m_max_density = dr::opaque<Float>(m_scale * m_sigmat->max());
        update_majorant_grid(m_sigmat.get(), m_scale);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("scale",   m_scale,  ParamFlags::NonDifferentiable);
        if (m_sigmas)
            cb->put("sigma_s", m_sigmas, ParamFlags::Differentiable);
        else
            cb->put("albedo",  m_albedo, ParamFlags::Differentiable);
        cb->put("sigma_t", m_sigmat, ParamFlags::Differentiable);
        Base::traverse(cb);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        m_max_density = dr::opaque<Float>(m_scale * m_sigmat->max());
        update_majorant_grid(m_sigmat.get(), m_scale);
    }

    UnpolarizedSpectrum
    get_majorant(const MediumInteraction3f &mi,
                 Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        if (has_majorant_grid())
            return UnpolarizedSpectrum(majorant_grid_eval(mi.p, active)) &
                   active;
        DRJIT_MARK_USED(mi);
        return m_max_density;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        auto sigmat = m_scale * m_sigmat->eval(mi, active);
        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigmat *= m_phase_function->projected_area(mi, active);

        UnpolarizedSpectrum sigmas =
            m_sigmas ? UnpolarizedSpectrum(m_scale * m_sigmas->eval(mi, active))
                     : UnpolarizedSpectrum(sigmat * m_albedo->eval(mi, active));
        UnpolarizedSpectrum majorant =
            has_majorant_grid()
                ? UnpolarizedSpectrum(majorant_grid_eval(mi.p, active))
                : UnpolarizedSpectrum(m_max_density);
        auto sigman = dr::maximum(majorant - sigmat, 0.f);
        return { sigmas, sigman, sigmat };
    }

    UnpolarizedSpectrum
    get_albedo(const MediumInteraction3f &mi, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        if (m_albedo)
            return m_albedo->eval(mi, active) & active;
        /* The ratio; the scale cancels. Undefined where nothing absorbs, so
           report zero there as the base implementation does. */
        UnpolarizedSpectrum sigmat = m_sigmat->eval(mi, active),
                            sigmas = m_sigmas->eval(mi, active);
        return dr::select(sigmat > 0.f, sigmas / sigmat, 0.f) & active;
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_sigmat->bbox().ray_intersect(ray);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HeterogeneousMedium[" << std::endl
            << (m_sigmas ? "  sigma_s = " : "  albedo  = ")
            << string::indent(m_sigmas ? m_sigmas : m_albedo) << std::endl
            << "  sigma_t = " << string::indent(m_sigmat) << std::endl
            << "  scale   = " << string::indent(m_scale) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(HeterogeneousMedium)
private:
    ref<Volume> m_sigmat, m_albedo, m_sigmas;
    ScalarFloat m_scale;
    Float m_max_density;

    MI_TRAVERSE_CB(Base, m_sigmat, m_albedo, m_sigmas, m_max_density)
};

MI_EXPORT_PLUGIN(HeterogeneousMedium)
NAMESPACE_END(mitsuba)
