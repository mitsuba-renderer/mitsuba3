#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>
#include <drjit/traversable_base.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-area:

Area light (:monosp:`area`)
---------------------------

.. pluginparameters::

 * - radiance
   - |spectrum| or |texture|
   - Specifies the emitted radiance in units of power per unit area per unit steradian.
   - |exposed|, |differentiable|

 * - twosided
   - |bool|
   - Emit light from both sides of the surface. The default emits only into
     the hemisphere containing the surface normal. Enabling this parameter
     doubles the emitted power for a given radiance value. (Default: |false|)

This plugin implements an area light, i.e. a light source that emits
diffuse illumination from the exterior of an arbitrary shape.
Since the emission profile of an area light is completely diffuse, it
has the same apparent brightness regardless of the observer's viewing
direction. Furthermore, since it occupies a nonzero amount of space, an
area light generally causes scene objects to cast soft shadows.

The :ref:`visibility <sec-shape-visibility>` of an area light is a property
of the shape that carries it.

To create an area light source, simply instantiate the desired
emitter shape and specify an :monosp:`area` instance as its child:

.. tabs::
    .. code-tab:: xml
        :name: sphere-light

        <shape type="sphere">
            <emitter type="area">
                <rgb name="radiance" value="1.0"/>
            </emitter>
        </shape>

    .. code-tab:: python

        'type': 'sphere',
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'rgb',
                'value': 1.0,
            }
        }

 */

template <typename Float, typename Spectrum>
class AreaLight final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_shape, m_medium)
    MI_IMPORT_TYPES(Scene, Shape, Texture)

    AreaLight(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The area light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.get_emissive_texture<Texture>("radiance", 1.f);
        m_twosided = props.get<bool>("twosided", false);

        m_flags = +EmitterFlags::Surface;
        if (m_radiance->is_spatially_varying())
            m_flags |= +EmitterFlags::SpatiallyVarying;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("radiance", m_radiance, ParamFlags::Differentiable);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        Spectrum result = depolarizer<Spectrum>(m_radiance->eval(si, active));
        if (!m_twosided)
            result = dr::select(Frame3f::cos_theta(si.wi) > 0.f, result, 0.f);
        return result;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        auto [ps, pos_weight] = sample_position(time, sample2, active);

        // 2. Sample directional component. Two-sided emitters reuse the first
        // sample dimension to pick a side.
        Vector3f local;
        if (m_twosided) {
            Point2f sample_dir = sample3;
            Mask flip = sample_dir.x() >= .5f;
            sample_dir.x() = dr::select(flip, dr::fmadd(sample_dir.x(), 2.f, -1.f),
                                        sample_dir.x() * 2.f);
            local = warp::square_to_cosine_hemisphere(sample_dir);
            dr::masked(local.z(), flip) = -local.z();
        } else {
            local = warp::square_to_cosine_hemisphere(sample3);
        }

        // 3. Sample spectral component
        SurfaceInteraction3f si(ps, dr::zeros<Wavelength>());
        auto [wavelength, wav_weight] =
            sample_wavelengths(si, wavelength_sample, active);
        si.time = time;
        si.wavelengths = wavelength;

        // Note: some terms cancelled out with `warp::square_to_cosine_hemisphere_pdf`.
        Spectrum weight = pos_weight * wav_weight *
                          (m_twosided ? 2.f : 1.f) * dr::Pi<ScalarFloat>;

        return { si.spawn_ray(si.to_world(local)),
                 depolarizer<Spectrum>(weight) };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        if constexpr (drjit::is_jit_v<Float>) {
            if (!m_shape)
                return { dr::zeros<DirectionSample3f>(), 0.f };
        } else {
            Assert(m_shape, "Can't sample from an area emitter without an "
                            "associated Shape.");
        }

        DirectionSample3f ds;
        SurfaceInteraction3f si;

        // One of two very different strategies is used depending on 'm_radiance'
        if (likely(!m_radiance->is_spatially_varying())) {
            // Texture is uniform, try to importance sample the shape wrt. solid angle at 'it'
            ds = m_shape->sample_direction(it, sample, active);
            active &= ds.pdf != 0.f;
            if (!m_twosided)
                active &= dr::dot(ds.d, ds.n) < 0.f;

            si = SurfaceInteraction3f(ds, it.wavelengths);
        } else {
            // Importance sample the texture, then map onto the shape
            auto [uv, pdf] = m_radiance->sample_position(sample, active);
            active &= (pdf != 0.f);

            si = m_shape->eval_parameterization(
                uv, +RayFlags::FollowShape | +RayFlags::Default, active);
            si.wavelengths = it.wavelengths;
            active &= si.is_valid();

            ds.p = si.p;
            ds.n = si.n;
            ds.uv = si.uv;
            ds.time = it.time;
            ds.delta = false;
            ds.d = ds.p - it.p;

            Float dist_squared = dr::squared_norm(ds.d);
            ds.dist = dr::sqrt(dist_squared);
            ds.d /= ds.dist;

            Float dp = dr::dot(ds.d, ds.n);
            if (m_twosided)
                dp = -dr::abs(dp);
            active &= dp < 0.f;
            ds.pdf = dr::select(active, pdf / dr::norm(dr::cross(si.dp_du, si.dp_dv)) *
                                        dist_squared / -dp, 0.f);
        }

        UnpolarizedSpectrum spec = m_radiance->eval(si, active) / ds.pdf;
        ds.emitter = this;
        return { ds, depolarizer<Spectrum>(spec) & active };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        Float dp = dr::dot(ds.d, ds.n);
        if (m_twosided)
            dp = -dr::abs(dp);
        active &= dp < 0.f;

        if constexpr (drjit::is_jit_v<Float>) {
            if (!m_shape)
                return 0.f;
        } else {
            Assert(m_shape,
                   "The area emitter without has no associated Shape!");
        }

        Float value;
        if (!m_radiance->is_spatially_varying()) {
            value = m_shape->pdf_direction(it, ds, active);
        } else {
            // This surface intersection would be nice to avoid..
            SurfaceInteraction3f si = m_shape->eval_parameterization(ds.uv, +RayFlags::Default, active);
            active &= si.is_valid();

            value = m_radiance->pdf_position(ds.uv, active) * dr::square(ds.dist) /
                    (dr::norm(dr::cross(si.dp_du, si.dp_dv)) * -dp);
        }

        return dr::select(active, value, 0.f);
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        Float dp = dr::dot(ds.d, ds.n);
        if (m_twosided)
            dp = -dr::abs(dp);
        active &= dp < 0.f;

        SurfaceInteraction3f si(ds, it.wavelengths);

        if constexpr (dr::is_diff_v<Float>) {
            // The sample is fixed. When the shape moves, evaluate the texture
            // where the direction meets the moved surface.
            if (m_shape && m_radiance->is_spatially_varying() &&
                m_shape->parameters_grad_enabled()) {
                SurfaceInteraction3f si_m = m_shape->eval_parameterization(
                    ds.uv, +RayFlags::FollowShape | +RayFlags::Default, active);
                si.uv += uv_slide(si_m, dr::detach(ds.d), active);
            }
        }

        UnpolarizedSpectrum spec = m_radiance->eval(si, active);
        return dr::select(active, depolarizer<Spectrum>(spec), 0.f);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);

        if constexpr (drjit::is_jit_v<Float>) {
            if (!m_shape)
                return { dr::zeros<PositionSample3f>(), 0.f };
        } else {
            Assert(m_shape, "Cannot sample from an area emitter without an "
                            "associated Shape.");
        }

        // Two strategies to sample the spatial component based on 'm_radiance'
        PositionSample3f ps;
        if (!m_radiance->is_spatially_varying()) {
            // Radiance not spatially varying, use area-based sampling of shape
            ps = m_shape->sample_position(time, sample, active);
        } else {
            // Importance sample texture
            auto [uv, pdf] = m_radiance->sample_position(sample, active);
            active &= (pdf != 0.f);

            auto si = m_shape->eval_parameterization(uv, +RayFlags::Default, active);
            active &= si.is_valid();
            pdf /= dr::norm(dr::cross(si.dp_du, si.dp_dv));

            ps = si;
            ps.pdf = pdf;
            ps.delta = false;
        }

        Float weight = dr::select(active && ps.pdf > 0.f, dr::rcp(ps.pdf), Float(0.f));
        return { ps, weight };
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        return m_radiance->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
    }

    ScalarBoundingBox3f bbox() const override { return m_shape->bbox(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "AreaLight[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  twosided = " << m_twosided << "," << std::endl
            << "  surface_area = ";
        if (m_shape) oss << m_shape->surface_area();
        else         oss << "  <no shape attached!>";
        oss << "," << std::endl;
        if (m_medium) oss << string::indent(m_medium);
        else         oss << "  <no medium attached!>";
        oss << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(AreaLight)
private:
    /**
     * Derivative of the texture coordinates seen along a fixed direction
     *
     * A detached emitter sample fixes the direction ``d`` from the reference
     * point. When the shape moves, this direction meets the surface at a
     * different location. The function returns the resulting change of the
     * texture coordinates at ``si`` as a zero-valued quantity whose derivative
     * is the first-order displacement, so that it can be added to ``si.uv``.
     *
     * Args:
     *     si: Surface interaction at the sample, evaluated with
     *         `RayFlags.FollowShape` so that the derivative of ``si.p`` is
     *         the motion of the surface at fixed texture coordinates
     *
     *     d: Detached unit direction from the reference point to the sample
     *
     *     active: Mask of active lanes
     *
     * Returns:
     *     The zero-valued change of the texture coordinates
     */
    Point2f uv_slide(const SurfaceInteraction3f &si, const Vector3f &d,
                     Mask active) const {
        // Point where the fixed direction meets the moving tangent plane,
        // relative to the moving point (see Mesh::compute_surface_interaction)
        Vector3f dp = si.p - dr::detach(si.p),
                 n  = dr::detach(si.n);
        Float n_d = dr::select(active, dr::dot(n, d), 1.f);
        Vector3f rel = d * (dr::dot(n, dp) / n_d) - dp;

        // Least squares solution of [dp_du, dp_dv] * duv = rel
        Vector3f e1 = dr::detach(si.dp_du), e2 = dr::detach(si.dp_dv);
        Float a11 = dr::dot(e1, e1), a12 = dr::dot(e1, e2),
              a22 = dr::dot(e2, e2), r1 = dr::dot(e1, rel),
              r2  = dr::dot(e2, rel),
              det = dr::select(active, dr::fmsub(a11, a22, a12 * a12), 1.f);
        return Point2f(dr::fmsub(a22, r1, a12 * r2),
                       dr::fnmadd(a12, r1, a11 * r2)) / det;
    }

    ref<Texture> m_radiance;
    bool m_twosided;

    MI_TRAVERSE_CB(Base, m_radiance)
};

MI_EXPORT_PLUGIN(AreaLight)
NAMESPACE_END(mitsuba)
