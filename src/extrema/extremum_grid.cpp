#include <mitsuba/core/properties.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/extremum.h>
#include <mitsuba/render/volume.h>
#include <nanothread/nanothread.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _extremum-extremum_grid:

Extremum grid structure (:monosp:`extremum_grid`)
-------------------------------------------------

.. pluginparameters::

 * - resolution
   - |vector|
   - Grid resolution along the XYZ axis. Does not have to be a multiple of
     the underlying volume. Default: [1,1,1]

This plugin creates a regular grid structure storing local extrema value.
The grid is constructed by querying a dense grid of bounding boxes corresponding
to the volumes cells.
``traverse_extremum`` uses a DDA (Digital Differential Analyzer) to efficiently
track through the grid.
*/

template <typename Float, typename Spectrum>
class ExtremumGrid final : public Extremum<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Extremum, m_bbox, m_scale)
    MI_IMPORT_TYPES(Volume)

    using TrackingStateType    = TrackingState<Float, Spectrum>;
    using TrackingFunctionType = TrackingFunction<Float, Spectrum>;
    using FloatStorage         = DynamicBuffer<Float>;

    ExtremumGrid(const Properties &props) : Base(props) {

        // Resolution Parameters
        m_resolution = props.get<ScalarVector3i>("resolution", ScalarVector3i(1,1,1));
    }

    void build(const Volume *volume) override {

        VolumeParametrization<ScalarFloat> volume_param = volume->parametrization();

        m_to_local = volume_param.to_world.inverse();

        ScalarVector3i clamped  = dr::clip(m_resolution, 1, volume->resolution());
        if (dr::any(clamped != m_resolution)) {
            Log(Info,
                "ExtremumGrid: requested resolution %s is finer than the "
                "underlying volume's resolution %s; clamping to %s.",
                m_resolution, volume->resolution(), clamped);
            m_resolution = clamped;
        }

        build_grid(volume, m_resolution);
    }

    TrackingStateType traverse_extremum(
        const Ray3f &ray,
        Float mint,
        Float maxt,
        UInt32 channel,
        TrackingStateType state,
        const TrackingFunctionType &func,
        Mask active
    ) const override {
        return traverse_dda(
            func,
            state,
            ray,
            mint,
            maxt,
            channel,
            active
        );
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("resolution", m_resolution, ParamFlags::NonDifferentiable);
        Base::traverse(cb);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ExtremumGrid[" << std::endl
            << "  resolution = " << m_resolution << "," << std::endl
            << "  bbox = " << m_bbox << "," << std::endl
            << "  scale = " << m_scale << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(ExtremumGrid)

private:

    /**
     * \brief Build the extremum grid from a volume
     *
     * This method constructs a lower-resolution grid where each cell stores
     * the majorant (maximum) extinction value over the corresponding region
     * of the high-resolution volume.
     */
    void build_grid(const Volume *volume, ScalarVector3i resolution) {

        // local space supergrid cell size
        const ScalarVector3f cell_size = dr::rcp(ScalarVector3f(resolution));

        ScalarVector2f safety_factor(1.f - dr::Epsilon<Float>,
                                     1.f + dr::Epsilon<Float>);

        // Allocate extremum grid data
        size_t n = dr::prod(resolution);

        size_t n_threads  = pool_size() + 1;
        size_t grain_size = std::max(n / (4 * n_threads), (size_t) 1);

        m_extremum_grid = dr::empty<FloatStorage>(n * 2);

        // Early return if using the global majorant.
        if (n == 1) {
            ScalarFloat max = volume->max();
            // Global minorant tracking is not yet implemented on the volume
            // side; use a placeholder minorant until it lands.
            ScalarFloat min = 0.f;
            dr::scatter(m_extremum_grid,
                        Vector2f(min, max) * safety_factor,
                        UInt32(0));
            return;
        }

        if constexpr (!dr::is_jit_v<Float>) {
            auto guard = volume->pin();

            dr::parallel_for(
                dr::blocked_range<size_t>(0, n, grain_size),
                [&](const dr::blocked_range<size_t> &range) {
                    // Recover x, y, z from block start (one-time div/mod per
                    // block)

                    for (auto idx = range.begin(); idx != range.end(); ++idx) {
                        // Store in linear array (Z-slowest, X-fastest)
                        int32_t x = idx % resolution.x();
                        int32_t y = (idx / resolution.x()) % resolution.y();
                        int32_t z = idx / (resolution.x() * resolution.y());

                        ScalarPoint3f cell_min =
                            ScalarVector3f(x, y, z) * cell_size;
                        ScalarPoint3f cell_max = cell_min + cell_size;
                        ScalarBoundingBox3f cell_bounds(
                            cell_min + math::RayEpsilon<Float>,
                            cell_max - math::RayEpsilon<Float>);

                        // Query volume for local extremum, currently assume
                        // local bounds.
                        auto [min, maj] = volume->extremum(cell_bounds);

                        dr::scatter(m_extremum_grid,
                                    Vector2f(min, maj) * safety_factor,
                                    UInt32(idx));
                    }
                });
        } else {

            UInt32 idx = dr::arange<UInt32>((uint32_t) n);

            UInt32 x = idx % resolution.x() ;
            UInt32 y = (idx / resolution.x())  % resolution.y();
            UInt32 z =  idx / (resolution.x() * resolution.y());

            Point3f cell_min = Vector3f(x, y, z) * cell_size;
            Point3f cell_max = cell_min + cell_size;
            BoundingBox3f cell_bounds(
                            cell_min + math::RayEpsilon<Float>,
                            cell_max - math::RayEpsilon<Float>
                        );

            auto [min, maj]= volume->extremum(cell_bounds);

            dr::scatter(m_extremum_grid, min * safety_factor.x(), idx*2);
            dr::scatter(m_extremum_grid, maj * safety_factor.y(), idx*2+1);
            dr::sync_thread();
        }

        Log(Info, "Extremum grid constructed successfully");
    }

    /** \brief General regular grid DDA traversal algorithm.
     *
     * This method traverses the regular grid along the provided ray using the
     * DDA algorithm.
     *
     * \param func  Function to be called at each step of the traversal.
     *              Must be of type \ref TrackingFunction (tracking.h).
     * \param state The payload passed to ``func``.
     * \param ray   The ray along which the structure is traversed.
     * \param mint  The minimum distance along the ray.
     * \param maxt  The maximum distance along the ray.
     * \param active
     *
     * \return
     *      Returns the final state at the end of the traversal.
     */
    template<typename FuncT, typename StateT>
    std::decay_t<StateT> traverse_dda(
        FuncT&& func,
        StateT&& state,
        const Ray3f& ray,
        Float mint,
        Float maxt,
        UInt32 channel,
        Mask active
    ) const {
        using StateD = std::decay_t<StateT>;

        ExtremumSegment segment = dr::zeros<ExtremumSegment>();

        // Currently assuming that the majorant aligns perfectly with the
        // volume and that values outside the bbox cannot be evaluated.
        // Transform ray to local grid coordinates [0,res]³
        Vector3f res = Vector3f(m_resolution);
        Ray3f local_ray((m_to_local * ray.o) * res, // Normalize origin
                        (m_to_local * ray.d) * res, // Normalize direction
                        ray.time, ray.wavelengths);
        Vector3f rcp_d = dr::rcp(local_ray.d);
        auto inf_t     = local_ray.d == 0.f;
        auto d_pos     = local_ray.d >= 0.f;

        Float t_min = mint;
        Float t_max = maxt;

        active &= t_max > t_min && dr::isfinite(t_max);

        // Advance the ray to the start of the interval
        local_ray.o = dr::fmadd(local_ray.d, t_min, local_ray.o);
        t_max       = t_max - t_min;
        t_min       = 0.f;

        // Compute the integer step direction
        Vector3i step      = dr::select(d_pos, 1, -1);
        Vector3f abs_rcp_d = abs(rcp_d);

        // Integer grid coordinates
        Vector3i pi = dr::floor2int<Vector3i>(local_ray.o);

        // Fractional entry position
        Vector3f p0 = local_ray.o - Vector3f(pi);
        // Step size to next interaction
        Vector3f dt_v =
            dr::select(d_pos, dr::fmadd(-p0, rcp_d, rcp_d), -p0 * rcp_d);
        dr::masked(dt_v, inf_t) = dr::Infinity<Float>;

        struct LoopState {
            ExtremumSegment segment;
            StateD state;
            Mask advance;
            Mask active;
            Vector3f dt_v;
            Vector3i pi;
            Float t_rem;

            DRJIT_STRUCT(LoopState, segment, state, advance, active, dt_v, \
                pi, t_rem)
        } ls = {
            segment,
            state,
            /*advance=*/active,
            active,
            dt_v,
            pi,
            t_max
        };

        dr::tie(ls) = dr::while_loop(
            dr::make_tuple(ls),
            [](const LoopState& ls) { return ls.active; },
            [this, func, step, abs_rcp_d, t_max, mint, channel](LoopState& ls) {

            ExtremumSegment& segment = ls.segment;
            StateD& state = ls.state;
            Mask& advance    = ls.advance;
            Mask& active    = ls.active;
            Vector3f& dt_v  = ls.dt_v;
            Vector3i& pi    = ls.pi;
            Float& t_rem = ls.t_rem;

            // Select the smallest step.
            Float dt  = dr::minimum(dr::min(dt_v), t_rem);
            auto mask = dt_v == dt;

            // Note: not multiplying the index by 2 because we gather using
            // Vector2f.
            Vector3i piw = dr::clip(pi, 0, m_resolution - 1);
            UInt32 idx = dr::fmadd(dr::fmadd(piw.z(), m_resolution.y(), piw.y()),
                                   m_resolution.x(), piw.x());

            const Vector2f extremum =
                m_scale * dr::gather<Vector2f>(m_extremum_grid, idx);

            // Store segment for lanes that reached target
            Float t_curr = mint + t_max - t_rem;
            dr::masked(segment, active) = ExtremumSegment(
                t_curr,
                t_curr + dt,
                extremum
            );

            std::tie(advance, active) = func(segment, &state, channel, active);

            // Advance
            dr::masked(dt_v, advance) = dr::select(mask, abs_rcp_d, dt_v - dt);
            dr::masked(pi, mask && advance) += step;
            dr::masked(t_rem, advance) -= dt;

            active &= t_rem > 0.f;
        },
        "DDA Traversal");

        return ls.state;
    };

private:
    /// Grid storing pre-computed local majorants
    FloatStorage m_extremum_grid;
    ScalarVector3i m_resolution;
    ScalarAffineTransform4f m_to_local;
};

MI_EXPORT_PLUGIN(ExtremumGrid)
NAMESPACE_END(mitsuba)
