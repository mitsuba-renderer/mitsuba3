#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)
/**!

.. _emitter-smootharea:

Smooth area light (:monosp:`smootharea`)
----------------------------------------

.. pluginparameters::

 * - radiance
   - |spectrum|
   - Specifies the emitted radiance in units of power per unit area per unit steradian.
     (Default: :ref:`d65 <emitter-d65>`)
 * - blur_size
   - |float|
   - Specifies the width of the smooth transition region from full emission to zero
     at the borders of the area light, in uv space. (Default: 0.1)

This plugin implements an area light with a smooth transition from full emission
to zero (black) at its borders. This type of light is useful for differentiable
rendering since it typically avoids discontinuities around area lights. The transition
region is defined in uv space. This plugin should be used with a flat quadrilateral mesh
with texture coordinates that map to the unit square.

 */

template <typename Float, typename Spectrum>
class SmoothAreaLight final : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_shape, m_medium)
    MI_IMPORT_TYPES(Scene, Shape, Texture)

    SmoothAreaLight(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The area light inherits this transformation from its parent "
                  "shape.");

        m_radiance = props.texture<Texture>("radiance", Texture::D65(1.f));
        if (is_spectral_v<Spectrum> && m_radiance->is_spatially_varying()) {
            // TODO: this should probably be done in the parser, just like with
            // non-textured spectra.
            m_d65 = Texture::D65(1.f);
        }

        m_blur_size = props.get<ScalarFloat>("blur_size", 0.1f);

        m_flags = +EmitterFlags::Surface;
        if (m_radiance->is_spatially_varying())
            m_flags |= +EmitterFlags::SpatiallyVarying;
        dr::set_attr(this, "flags", m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("radiance", m_radiance.get(), +ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (string::contains(keys, "parent"))
            m_area_times_pi = m_shape->surface_area() * dr::Pi<ScalarFloat>;
    }

    void set_shape(Shape *shape) override {
        if (m_shape)
            Throw("An area emitter can be only be attached to a single shape.");

        Base::set_shape(shape);
        m_area_times_pi = m_shape->surface_area() * dr::Pi<ScalarFloat>;
    }

    Float smooth_profile(Float x) const {
        Float res(0);
        dr::masked(res, x >= m_blur_size && x <= Float(1) - m_blur_size) = Float(1);
        dr::masked(res, x < m_blur_size && x > Float(0)) = x / m_blur_size;
        dr::masked(res, x > Float(1) - m_blur_size && x < Float(1)) = (1 - x) / m_blur_size;
        return res;
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        auto result = dr::select(
            Frame3f::cos_theta(si.wi) > 0.f,
            depolarizer<Spectrum>(m_radiance->eval(si, active))
                * smooth_profile(si.uv.x()) * smooth_profile(si.uv.y()),
            0.f
        );

        if (is_spectral_v<Spectrum> && m_radiance->is_spatially_varying())
            result *= m_d65->eval(si, active);

        return result;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &sample2, const Point2f &sample3,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        PositionSample3f ps = m_shape->sample_position(time, sample2, active);

        // 2. Sample directional component
        Vector3f local = warp::square_to_cosine_hemisphere(sample3);

        // 3. Sample spectrum
        SurfaceInteraction3f si(ps, dr::zero<Wavelength>(0.f));
        auto [wavelength, wav_weight] =
            sample_wavelengths(si, wavelength_sample, active);

        wav_weight *= smooth_profile(ps.uv.x()) * smooth_profile(ps.uv.y());

        return std::make_pair(
            Ray3f(ps.p, Frame3f(ps.n).to_world(local), time, wavelength),
            depolarizer<Spectrum>(wav_weight) * m_area_times_pi
        );
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        Assert(m_shape, "Can't sample from an area emitter without an associated Shape.");

        DirectionSample3f ds = dr::zero<DirectionSample3f>();
        SurfaceInteraction3f si;

        // One of two very different strategies is used depending on 'm_radiance'
        if (likely(!m_radiance->is_spatially_varying())) {
            // Texture is uniform, try to importance sample the shape wrt. solid angle at 'it'
            ds = m_shape->sample_direction(it, sample, active);
            active &= dr::dot(ds.d, ds.n) < 0.f && dr::neq(ds.pdf, 0.f);

            si = SurfaceInteraction3f(ds, it.wavelengths);
        } else {
            // Importance sample the texture, then map onto the shape
            auto [uv, pdf] = m_radiance->sample_position(sample, active);
            active &= dr::neq(pdf, 0.f);

            si = m_shape->eval_parameterization(uv, +RayFlags::All, active);
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
            active &= dp < 0.f;
            ds.pdf = dr::select(active, pdf / dr::norm(dr::cross(si.dp_du, si.dp_dv)) *
                                        dist_squared / -dp, 0.f);
        }

        Spectrum spec = m_radiance->eval(si, active) / ds.pdf;
        spec *= smooth_profile(ds.uv.x()) * smooth_profile(ds.uv.y());

        if (is_spectral_v<Spectrum> && m_radiance->is_spatially_varying())
            spec *= m_d65->eval(si, active);

        ds.emitter = this;
        return { ds, depolarizer<Spectrum>(spec) & active };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return dr::select(dr::dot(ds.d, ds.n) < 0.f,
                          m_shape->pdf_direction(it, ds, active), 0.f);
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        /* We need to recompute the UV coordinates at the sampled location on
           the emitter in order for their derivatives to be tracked properly
           when either the reference interaction or the sampled direction record
           have gradient tracking enabled. */
        Point2f uv = ds.uv;
        if constexpr (dr::is_diff_array_v<Float>) {
            if (dr::grad_enabled(it.p, ds.p)) {
                PreliminaryIntersection3f pi(it, ds, m_shape);
                Ray3f ray(it.p, ds.d, it.time, it.wavelengths);

                auto si = m_shape->compute_surface_interaction(
                    ray, pi, +RayFlags::UV, 0, active);

                uv = dr::replace_grad(uv, si.uv);
            }
        }

        Float dp = dr::dot(ds.d, ds.n);
        active &= dp < 0.f;

        SurfaceInteraction3f si(ds, it.wavelengths);
        UnpolarizedSpectrum spec = m_radiance->eval(si, active);
        spec *= smooth_profile(uv.x()) * smooth_profile(uv.y());

        if (is_spectral_v<Spectrum> && m_radiance->is_spatially_varying())
            spec *= m_d65->eval(si, active);

        return dr::select(active, depolarizer<Spectrum>(spec), 0.f);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSamplePosition, active);
        Assert(m_shape, "Cannot sample from an area emitter without an associated Shape.");

        // Two strategies to sample the spatial component based on 'm_radiance'
        PositionSample3f ps = dr::zero<PositionSample3f>();
        if (!m_radiance->is_spatially_varying()) {
            // Radiance not spatially varying, use area-based sampling of shape
            ps = m_shape->sample_position(time, sample, active);
        } else {
            // Importance sample texture
            auto [uv, pdf] = m_radiance->sample_position(sample, active);
            active &= dr::neq(pdf, 0.f);

            auto si = m_shape->eval_parameterization(uv, +RayFlags::All, active);
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
        auto [wav, weight] = m_radiance->sample_spectrum(
            si, math::sample_shifted<Wavelength>(sample), active);
        if (is_spectral_v<Spectrum> && m_radiance->is_spatially_varying()) {
            SurfaceInteraction3f si2(si);
            si2.wavelengths = wav;
            weight *= m_d65->eval(si2, active);
        }
        return { wav, weight };
    }

    ScalarBoundingBox3f bbox() const override { return m_shape->bbox(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothAreaLight[" << std::endl
            << "  radiance = " << string::indent(m_radiance) << "," << std::endl
            << "  surface_area = ";
        if (m_shape) oss << m_shape->surface_area();
        else         oss << "  <no shape attached!>";
        oss << "," << std::endl;
        if (m_medium) oss << string::indent(m_medium->to_string());
        else         oss << "  <no medium attached!>";
        oss << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_radiance, m_d65;
    Float m_area_times_pi = 0.f;
    ScalarFloat m_blur_size;
};

MI_IMPLEMENT_CLASS_VARIANT(SmoothAreaLight, Emitter)
MI_EXPORT_PLUGIN(SmoothAreaLight, "Smooth Area emitter")
NAMESPACE_END(mitsuba)