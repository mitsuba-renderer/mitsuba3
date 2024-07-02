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
#include <mitsuba/core/distr_1d.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _medium-piecewise:

Piecewise medium (:monosp:`piecewise`)
-----------------------------------------------

.. pluginparameters::

 * - albedo
   - |float|, |spectrum| or |volume|
   - Single-scattering albedo of the medium (Default: 0.75).
   - |exposed|, |differentiable|

 * - sigma_t
   - |float|, |spectrum| or |volume|
   - Extinction coefficient in inverse scene units (Default: 1).
     The supplied grid must be of shape [1,1,n].
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
   - |exposed|, |differentiable|


This plugin provides a 1D heterogenous medium implementation (plane parallel geometry),  
which acquires its data from nested volume instances. These can be constant, 
use a procedural function, or fetch data from disk, e.g. using a 3D grid, as long as the
underlying grid has the shape [1,1,N], with N the number of layers on the z axis.

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
        :name: lst-piecewise

        <!-- Declare a piecewise participating medium named 'smoke' -->
        <medium type="piecewise" id="smoke">
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

        # Declare a piecewise participating medium named 'smoke'
        'smoke': {
            'type': 'piecewise',

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
class PiecewiseMedium final : public Medium<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Medium, m_is_homogeneous, m_has_spectral_extinction,
                    m_phase_function)
    MI_IMPORT_TYPES(Scene, Sampler, Texture, Volume)
    
    // Use 32 bit indices to keep track of indices to conserve memory
    using ScalarIndex = uint32_t;
    using ScalarSize  = uint32_t;
    using FloatStorage   = DynamicBuffer<Float>;
    using SpectrumStorage   = DynamicBuffer<UnpolarizedSpectrum>;

    PiecewiseMedium(const Properties &props) : Base(props) {

        m_is_homogeneous = false;
        m_albedo = props.volume<Volume>("albedo", 0.75f);
        m_sigmat = props.volume<Volume>("sigma_t", 1.f);

        m_scale = props.get<ScalarFloat>("scale", 1.0f);
        m_has_spectral_extinction = props.get<bool>("has_spectral_extinction", true);

        m_max_density = dr::opaque<Float>(m_scale * m_sigmat->max());

        precompute_optical_thickness();

        dr::set_attr(this, "is_homogeneous", m_is_homogeneous);
        dr::set_attr(this, "has_spectral_extinction", m_has_spectral_extinction);
    }

 std::tuple<typename Medium<Float, Spectrum>::MediumInteraction3f, Float, Float>
    sample_interaction_real(const Ray3f &ray, const SurfaceInteraction3f &si,
                            Float sample, UInt32 channel, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, active);

        using Index = dr::replace_scalar_t<Float, ScalarIndex>;

        // Initial intersection with the medium
        auto [aabb_its, mint, maxt] = intersect_aabb(ray);
        aabb_its &= (dr::isfinite(mint) || dr::isfinite(maxt));
        active &= aabb_its;
        dr::masked(mint, !active) = 0.f;
        dr::masked(maxt, !active) = dr::Infinity<Float>;

        mint = dr::maximum(0.f, mint);
        maxt = dr::minimum( si.t, dr::minimum(ray.maxt, maxt));
        
        Mask escaped = !active;

        // Initialize basic medium interaction fields
        MediumInteraction3f mei = dr::zeros<MediumInteraction3f>();
        mei.wi          = -ray.d;
        mei.sh_frame    = Frame3f(mei.wi);
        mei.time        = ray.time;
        mei.wavelengths = ray.wavelengths;
        mei.mint        = mint;
        mei.t           = mint; 
        mei.medium      = this;

        const ScalarVector3i res            = m_sigmat->resolution();
        const ScalarVector3f voxel_size     = m_sigmat->voxel_size();
        const ScalarVector3f inv_voxel_size = dr::rcp(voxel_size);

        const Vector3f step     = ScalarVector3f(0.f,0.f,voxel_size.z());
        const ScalarPoint3f min       = m_sigmat->bbox().min;
        
        const ScalarVector3f layer_norm( 0.f, 0.f, 1.f);
        Float cum_opt_thick     = dr::zeros<Float>();
        Float sampled_t         = dr::Infinity<Float>;
        Float tr  = dr::zeros<Float>();
        Float pdf = dr::zeros<Float>();
        
         // Calculate the distance between layers used as multiplication factor to the distance
        const Float n_dot_d   = dr::abs(dr::dot(layer_norm, dr::normalize(ray.d)));
        const Float delta     = dr::select(dr::eq(n_dot_d, 0.), dr::Infinity<Float>, voxel_size.z() / n_dot_d);
        const Float idelta    = dr::rcp(delta);     
        const Mask going_up   = ray.d.z() >= 0.f;

        Int32 start_idx = dr::clamp((Int32)dr::floor((ray(mint) - min) * inv_voxel_size).z(), 0, res.z() - 1);
        Int32 end_idx   = dr::clamp((Int32)dr::floor((ray(maxt) - min) * inv_voxel_size).z(), 0, res.z() - 1);
        Mask same_cell  = start_idx == end_idx;

        // make sure the index align with the array (reverse indices if going down)
        Index opt_start_idx = dr::select(going_up, start_idx, res.z() - 1 - start_idx);
        Index opt_end_idx   = dr::select(going_up, end_idx, res.z() - 1 - end_idx);
        Index index         = opt_start_idx;

        Float start_height  = (ray(mint + dr::Epsilon<Float>) - min).z() * inv_voxel_size.z() - (Float)start_idx;
        dr::masked(start_height, going_up) = 1.f - start_height;

        MediumInteraction3f s_mei = dr::zeros<MediumInteraction3f>();
        s_mei.p = ray(mint);
        std::tie(s_mei.sigma_s, s_mei.sigma_n, s_mei.sigma_t) = get_scattering_coefficients(s_mei, active);
        Float sigma_t = extract_channel(s_mei.sigma_t[0], channel);

        Float opt_thick_offset = dr::zeros<Float>();
        dr::masked( opt_thick_offset, going_up)  = extract_channel(dr::gather<Float>(m_cum_opt_thickness, opt_start_idx, active), channel);
        dr::masked( opt_thick_offset, !going_up) = extract_channel(dr::gather<Float>(m_reverse_cum_opt_thickness, opt_start_idx, active), channel);
        opt_thick_offset -= start_height * sigma_t;

        Float log_sample = dr::log(1. - sample);
        Mask sampled = dr::zeros<Mask>();
        Mask search = active && !same_cell;
        
        if(dr::any_or<true>(search))
        {
            UnpolarizedSpectrum spectral_value = dr::zeros<UnpolarizedSpectrum>();

            // Find the piecewise boundary in CDF space using binary search
            dr::masked(index, search) = dr::binary_search<Index>(
                opt_start_idx, opt_end_idx,
                [&](Index index) DRJIT_INLINE_LAMBDA {
                    dr::masked(spectral_value, going_up) = dr::gather<Float>(m_cum_opt_thickness, index, search);
                    dr::masked(spectral_value, !going_up) = dr::gather<Float>(m_reverse_cum_opt_thickness, index, search);

                    Float value = extract_channel(spectral_value, channel);

                    Float a = -log_sample * idelta; 
                    Float b = (value - opt_thick_offset);

                    return a > b;
                }
            );
        }

        same_cell |= index == opt_start_idx;
        search &= !same_cell;

        Int32 index_minus_one = dr::select(!active || same_cell, 0, index - 1);
        dr::masked(mei.t, search) += (start_height + (Float)( index- opt_start_idx - 1 )) * delta; 

        Float cum_opt_at_index = dr::zeros<Float>(); 
        dr::masked(cum_opt_at_index,going_up) =  extract_channel(dr::gather<Float>(m_cum_opt_thickness, index_minus_one, search), channel);
        dr::masked(cum_opt_at_index, !going_up) =  extract_channel(dr::gather<Float>(m_reverse_cum_opt_thickness, index_minus_one, search), channel);
        dr::masked(cum_opt_thick, search) = (cum_opt_at_index - opt_thick_offset) * delta;

        mei.p = min + voxel_size * 0.5 + 
                (Float)dr::select(going_up, index, res.z() - 1 - index)  * step;

        std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) = get_scattering_coefficients(mei, active && !same_cell);
        dr::masked(sigma_t, !same_cell) = extract_channel(mei.sigma_t, channel);

        escaped |= mei.t > maxt;
        sampled |= !escaped;

        dr::masked( sampled_t, sampled) = -dr::rcp(sigma_t) * (log_sample + cum_opt_thick ) + mei.t;

        escaped |= (sampled && sampled_t > maxt);
        sampled |= escaped;

        // need to calculate transmittance and pdf for escaped rays too
        dr::masked(sampled_t, sampled) = dr::select(escaped, maxt, sampled_t) ;
        dr::masked(tr, sampled) =  dr::exp( - (sampled_t - mei.t) * sigma_t - cum_opt_thick );
        dr::masked(pdf, sampled) = dr::select(dr::eq(sampled_t, maxt), tr, tr * sigma_t);
        
        mei.t = dr::select(!escaped, sampled_t, dr::Infinity<Float>);
        dr::masked(mei.p, !escaped)                     = ray(mei.t);
        dr::masked(mei.combined_extinction, !escaped)   = mei.sigma_t;
        dr::masked(mei.sigma_n, !escaped)               = dr::zeros<UnpolarizedSpectrum>();

        return {mei, tr, pdf};
    }

    std::tuple< Float, Float, Mask>
    eval_transmittance_pdf_real(const Ray3f &ray, const SurfaceInteraction3f &si,
                                    UInt32 channel, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        // Initial intersection with the medium
        auto [aabb_its, mint, maxt] = intersect_aabb(ray);
        aabb_its &= (dr::isfinite(mint) || dr::isfinite(maxt));
        active &= aabb_its;
        mint = dr::maximum(0.f, mint);
        Mask escaped = active && ((maxt >= ray.maxt) || (maxt >= si.t));

        maxt = dr::select( active, 
                           dr::minimum( ray.maxt , dr::minimum(maxt,  si.t)), 
                           dr::Infinity<Float> );
        maxt = dr::maximum(0.f, maxt);

        const ScalarVector3i res            = m_sigmat->resolution();
        const ScalarVector3f voxel_size     = m_sigmat->voxel_size();
        const ScalarVector3f inv_voxel_size = dr::rcp(voxel_size);
        
        const Vector3f step     = ScalarVector3f(0.f,0.f,voxel_size.z());
        const ScalarPoint3f min = m_sigmat->bbox().min;

         // Calculate the distance between layers used as multiplication factor to the distance
        const ScalarVector3f layer_norm( 0.f, 0.f, 1.f);
        const Float n_dot_d   =  dr::abs(dr::dot(layer_norm, dr::normalize(ray.d)));
        const Float delta     = dr::select(dr::eq(n_dot_d, 0.), dr::Infinity<Float>, voxel_size.z() / n_dot_d);
        const Mask going_up   = ray.d.z() >= 0.f;

        Int32 start_idx = dr::clamp((Int32)dr::floor((ray(mint) - min ) * inv_voxel_size).z(), 0, res.z() - 1);
        Int32 end_idx   = dr::clamp((Int32)dr::floor((ray(maxt) - min ) * inv_voxel_size).z(), 0, res.z() - 1);
        Mask same_cell  = start_idx == end_idx;
        
        Float start_height  = (ray(mint) - min).z() * inv_voxel_size.z() - (Float)start_idx;
        Float end_height    = (ray(maxt) - min).z() * inv_voxel_size.z() - (Float)end_idx;

        dr::masked(start_height, going_up)  = 1.f - start_height;
        dr::masked(end_height,   !going_up) = 1.f - end_height; 

        MediumInteraction3f s_mei = dr::zeros<MediumInteraction3f>();
        MediumInteraction3f e_mei = dr::zeros<MediumInteraction3f>();
    
        s_mei.p = min + voxel_size * 0.5 + start_idx * step;
        e_mei.p = min + voxel_size * 0.5 + end_idx * step;
        std::tie(s_mei.sigma_s, s_mei.sigma_n, s_mei.sigma_t) = get_scattering_coefficients(s_mei, active);
        std::tie(e_mei.sigma_s, e_mei.sigma_n, e_mei.sigma_t) = get_scattering_coefficients(e_mei, active);
        Float s_sigma_t = extract_channel(s_mei.sigma_t, channel);
        Float e_sigma_t = extract_channel(e_mei.sigma_t, channel);

        Int32 max_idx = dr::select(active, dr::maximum(dr::maximum(start_idx, end_idx) - 1, 0) , 0);
        Int32 min_idx = dr::select(active, dr::maximum(dr::minimum(start_idx, end_idx), 0), 0);
        Mask use_precomputed = active && max_idx > min_idx;

        Float opt_thick       = dr::zeros<Float>();
        Float cum_opt_thick   = dr::zeros<Float>();
        Float start_cum_opt   = dr::zeros<Float>();
        Float end_cum_opt     = dr::zeros<Float>();

        dr::masked( start_cum_opt, use_precomputed) = extract_channel(dr::gather<Float>(m_cum_opt_thickness, min_idx, use_precomputed), channel);
        dr::masked( end_cum_opt, use_precomputed)   = extract_channel(dr::gather<Float>(m_cum_opt_thickness, max_idx, use_precomputed), channel);
        dr::masked( cum_opt_thick, use_precomputed) = end_cum_opt - start_cum_opt;

        cum_opt_thick += s_sigma_t * start_height + e_sigma_t * end_height;
        cum_opt_thick *= delta;
        
        dr::masked(opt_thick, active) = dr::select(same_cell, (maxt - mint) * s_sigma_t, cum_opt_thick );

        Float tr  = dr::zeros<Float>();
        Float pdf = dr::zeros<Float>();
        dr::masked( tr,  active ) = dr::exp( -opt_thick );
        dr::masked( pdf, active ) = dr::select(si.t < maxt || ray.maxt < maxt, tr, tr * e_sigma_t);

        return { tr, pdf, escaped };
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("scale", m_scale,        +ParamFlags::NonDifferentiable);
        callback->put_object("albedo",   m_albedo.get(), +ParamFlags::Differentiable);
        callback->put_object("sigma_t",  m_sigmat.get(), +ParamFlags::Differentiable);
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        m_max_density = dr::opaque<Float>(m_scale * m_sigmat->max());
        Log(Info, "Medium Parameters changed!");
        precompute_optical_thickness();
    }
    
    void precompute_optical_thickness()
    {
        // Check that the first two dimensions are equal to 1 and that z is one or more.
        const ScalarVector3i resolution = m_sigmat->resolution();
        const ScalarVector3f voxel_size = m_sigmat->voxel_size();
        if(resolution.x() > 1 || resolution.y() > 1)
            Throw("PiecewiseMedium: x or y resolution bigger than one, assumed shape is [1,1,n]");

        MediumInteraction3f mei = dr::zeros<MediumInteraction3f>();
        ScalarPoint3f min       = m_sigmat->bbox().min;
        ScalarVector3f step     = ScalarVector3f(0.f,0.f,voxel_size.z());
        
        UnpolarizedSpectrum cumulative = dr::zeros<UnpolarizedSpectrum>();
        UnpolarizedSpectrum reverse_cumulative = dr::zeros<UnpolarizedSpectrum>();
        std::vector<UnpolarizedSpectrum> cum_opt_thickness(resolution.z());
        std::vector<UnpolarizedSpectrum> reverse_cum_opt_thickness(resolution.z());

        // loop through the layers and accumulate the "unscaled" optical thickness 
        for(int32_t i = 0; i < (int32_t)resolution.z(); ++i)
        {
            mei.p = min + voxel_size * 0.5 + (ScalarFloat)i * step;
            std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
            get_scattering_coefficients(mei, true);

            cumulative += mei.sigma_t;
            cum_opt_thickness[i] = cumulative;
        }

        for(int32_t i = (int32_t)resolution.z() - 1; i >= 0; --i)
        {
            mei.p = min + voxel_size * 0.5 + (ScalarFloat)i * step;
            std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
            get_scattering_coefficients(mei, true);

            reverse_cumulative += mei.sigma_t;
            reverse_cum_opt_thickness[(int32_t)resolution.z() - 1 - i] = reverse_cumulative;
        }

        m_cum_opt_thickness = dr::load<SpectrumStorage>( cum_opt_thickness.data(), resolution.z());
        m_reverse_cum_opt_thickness = dr::load<SpectrumStorage>( reverse_cum_opt_thickness.data(), resolution.z());
    }

    UnpolarizedSpectrum
    get_majorant(const MediumInteraction3f &mi,
                 Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
        
        auto sigmat = m_sigmat->eval(mi) * m_scale;
        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigmat *= m_phase_function->projected_area(mi, active);
        
        return m_max_density;
        return sigmat & active;
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi,
                                Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

        auto sigmat = m_scale * m_sigmat->eval(mi, active);
        if (has_flag(m_phase_function->flags(), PhaseFunctionFlags::Microflake))
            sigmat *= m_phase_function->projected_area(mi, active);

        auto sigmas = sigmat * m_albedo->eval(mi, active);
        auto sigman = m_max_density - sigmat;;
        return { sigmas, sigman, sigmat };
    }

    std::tuple<Mask, Float, Float>
    intersect_aabb(const Ray3f &ray) const override {
        return m_sigmat->bbox().ray_intersect(ray);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "PiecewiseMedium[" << std::endl
            << "  albedo        = " << string::indent(m_albedo) << std::endl
            << "  sigma_t       = " << string::indent(m_sigmat) << std::endl
            << "  scale         = " << string::indent(m_scale) << std::endl
            << "]";
        return oss.str();
    }

    static Float extract_channel(Spectrum value, UInt32 channel)
    {
        Float result = value[0];
        if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
            dr::masked(result, dr::eq(channel, 1u)) = value[1];
            dr::masked(result, dr::eq(channel, 2u)) = value[2];
        } else {
            DRJIT_MARK_USED(channel);
        }

        return result;
    }

    MI_DECLARE_CLASS()

private:
    ref<Volume> m_sigmat, m_albedo;
    ScalarFloat m_scale;

    Float m_max_density;
    SpectrumStorage m_cum_opt_thickness;
    SpectrumStorage m_reverse_cum_opt_thickness;
};

MI_IMPLEMENT_CLASS_VARIANT(PiecewiseMedium, Medium)
MI_EXPORT_PLUGIN(PiecewiseMedium, "Piecewise Medium")
NAMESPACE_END(mitsuba)
