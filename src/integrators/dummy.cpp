#include <enoki/morton.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Dummy integrator that always shoots the same (single) ray.
 * Does not produce useful images, but helps troubleshooting performance issues.
 */
class DummyIntegrator : public SamplingIntegrator {
public:
    // =============================================================
    //! @{ \name Constructors
    // =============================================================
    DummyIntegrator(const Properties &props) : SamplingIntegrator(props) {}

    /**
     * Trace always the exact same ray, independently of the RNG.
     */
    template <typename Value>
    void render_block_impl(const Scene *scene, Sampler *sampler,
                           ImageBlock *block, size_t sample_count_) const {
        using Point3          = Point<Value, 3>;
        using Vector2         = Vector<Value, 2>;
        using Spectrum        = mitsuba::Spectrum<Value>;
        using RadianceSample3 = RadianceSample<Point3>;

        const Sensor *sensor = scene->sensor();
        block->clear();
        uint32_t pixel_count = (uint32_t)(m_block_size * m_block_size);
        uint32_t sample_count =
            (uint32_t)(sample_count_ == (size_t) -1 ? sampler->sample_count()
                                                    : sample_count_);

        if ((sample_count % PacketSize) != 0)
            Throw("Sample count (%d) must be a multiple of packet size (%d)", sample_count, PacketSize);

        RadianceSample3 rs(scene, sampler);
        bool needs_time_sample  = sensor->shutter_open_time() > 0;
        Float diff_scale_factor = rsqrt((Float) sampler->sample_count());

        for (uint32_t i = 0; i < pixel_count && !should_stop(); ++i) {
            Point2u p = enoki::morton_decode<Point2u>(i);
            if (any(p >= block->size()))
                continue;

            for (uint32_t j = 0; j < sample_count && !should_stop();) {
                // Build (constant) ray
                Vector2 position_sample = 0.5f;
                Value aperture_sample   = 0.5f;
                Value time              = sensor->shutter_open();
                if (needs_time_sample)
                    time += 0.5f * sensor->shutter_open_time();
                Value wavelength_sample = 0.5f;

                auto [ray, ray_weight] = sensor->sample_ray_differential(
                    time, wavelength_sample, position_sample, aperture_sample);

                ray.scale_differential(diff_scale_factor);

                Spectrum result;
                /* Integrator::eval */ {
                    ScopedPhase sp(EProfilerPhase::ESamplingIntegratorEval);
                    mask_t<Value> active = any(ray_weight > 0.f);
                    result               = eval(ray, rs, active);
                }
                /* ImageBlock::put */ {
                    ScopedPhase sp(EProfilerPhase::EImageBlockPut);
                    auto position =
                        Point2f(p) + block->offset() + position_sample;
                    block->put(position, ray.wavelengths, ray_weight * result,
                               rs.alpha);
                }

                if constexpr (enoki::is_array_v<Value>)
                    j += Value::Size;
                else
                    ++j;
            }
        }
    }

    void render_block_scalar(const Scene *scene, Sampler *sampler,
                             ImageBlock *block,
                             size_t sample_count) const override {
        render_block_impl<Float>(scene, sampler, block, sample_count);
    }

    void render_block_vector(const Scene *scene, Sampler *sampler,
                             ImageBlock *block, Point2fX & /*points*/,
                             size_t sample_count) const override {
        render_block_impl<FloatP>(scene, sampler, block, sample_count);
    }

    template <typename RayDifferential,
              typename Value    = typename RayDifferential::Value,
              typename Point3   = typename RayDifferential::Point,
              typename Spectrum = mitsuba::Spectrum<Value>>
    Spectrum eval_impl(const RayDifferential &ray, RadianceSample<Point3> &rs,
                       mask_t<Value> active) const {
        using SurfaceInteraction = mitsuba::SurfaceInteraction<Point3>;

        const Scene *scene     = rs.scene;
        SurfaceInteraction &si = rs.ray_intersect(ray, active);
        rs.alpha               = select(si.is_valid(), Value(1.f), Value(0.f));

        // Visible emitters
        auto emitter = si.emitter(scene);
        active &= neq(emitter, nullptr);

        Spectrum result = 0.f;
        if (any(active))
            result[active] += emitter->eval(si, active);

        return result;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "DummyIntegrator[]";
        return oss.str();
    }

    MTS_IMPLEMENT_INTEGRATOR()
    MTS_DECLARE_CLASS()

private:
};

MTS_IMPLEMENT_CLASS(DummyIntegrator, SamplingIntegrator);
MTS_EXPORT_PLUGIN(DummyIntegrator, "DummyIntegrator");
NAMESPACE_END(mitsuba)
