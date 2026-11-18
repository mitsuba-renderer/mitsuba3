#include <mitsuba/core/properties.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/extremum.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/volumegrid.h>
#include <nanothread/nanothread.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _extremum-gridextremum:

Grid Extremum (:monosp:`gridextremum`)
--------------------------------------

.. pluginparameters::

 * - resolution
   - |vector|
   - Grid resolution along the XYZ axis. Does not have to be a multiple of
     the underlying volume. Default: [1,1,1]

This plugin creates a regular grid structure storing local extremum values for
efficient delta tracking in heterogeneous media. The grid is constructed
by querying the extinction volume's extrema over each grid cell.

At runtime, DDA (Digital Differential Analyzer) traversal through the grid provides
tight-fitting local extrema, dramatically reducing null collisions in media with
high spatial variance (*e.g.* clouds, fog).

Note that the current implementation expects :monosp:`to_world` and
:monosp:`scale` to match that of its volume and media to ensure correct extremum
bounds and periodic behaviours.;
*/

template <typename Float, typename Spectrum>
class GridExtremum final : public Extremum<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Extremum, m_bbox, m_scale)
    MI_IMPORT_TYPES(Volume)

    using FloatStorage         = DynamicBuffer<Float>;

    GridExtremum(const Properties &props) : Base(props) {
        // Resolution Parameters
        m_resolution = props.get<ScalarVector3i>("resolution", ScalarVector3i(1,1,1));
    }

    void build(const Volume *volume) override {
        // initialize volume specific parameters.
        VolumeParametrization<ScalarFloat> volume_param = volume->parametrization();
        m_to_local  = volume_param.to_world.inverse();
        m_wrap_mode = volume_param.wrap_mode;

        ScalarVector3i clamped  = dr::clip(m_resolution, 1, volume->resolution());
        if (dr::any(clamped != m_resolution)) {
            Log(Info,
                "GridExtremum: requested resolution %s is finer than the "
                "underlying volume's resolution %s; clamping to %s.",
                m_resolution, volume->resolution(), clamped);
            m_resolution = clamped;
        }

        for (size_t i = 0; i < 3; ++i)
            m_inv_resolution[i] = dr::divisor<int32_t>((int32_t) m_resolution[i]);

        build_grid(volume, m_resolution);
    }

    ExtremumState begin(const Ray3f &ray, Float mint, Float maxt,
                        Mask active) const override {
        auto [hit, d0, d1] = m_bbox.ray_intersect(ray);

        ExtremumState state = dr::zeros<ExtremumState>();
        Vector3f res(m_resolution);
        state.o = (m_to_local * ray.o) * res;
        state.d = (m_to_local * ray.d) * res;

        state.mint = dr::select(hit, dr::maximum(mint, d0), mint);
        state.maxt = dr::select(hit && active, dr::minimum(maxt, d1), state.mint);

        Vector3i pi =
            dr::floor2int<Vector3i>(dr::fmadd(state.d, state.mint, state.o));

        Mask clamped_bounds = m_wrap_mode == dr::WrapMode::Clamp;
        dr::masked(pi, clamped_bounds) = dr::clip(pi, -1, m_resolution);
        state.pi = pi;

        return state;
    }

    ExtremumSegment next(ExtremumState &state, const Ray3f &,
                         Mask active) const override {

        // Retrieve stepping direction and next distances
        Vector3i step  = dr::select(state.d >= 0.f, Vector3i(1), Vector3i(-1));
        Vector3i b_idx = state.pi + dr::maximum(step, 0);
        Vector3f ti    = (Vector3f(b_idx) - state.o) / state.d;

        // Handle traversal outside clamped bounds
        Mask clamped_bounds = m_wrap_mode == dr::WrapMode::Clamp;
        auto no_cross = state.d == 0.f;
        no_cross |= clamped_bounds && ((b_idx < 0) || (b_idx > m_resolution));
        dr::masked(ti, no_cross) = dr::Infinity<Float>;

        // Get closest intersection
        Float t_next = dr::minimum(dr::min(ti), state.maxt);
        dr::masked(t_next, !active) = state.maxt;

        // Retrieve flat index from the wrapped 3D index
        Vector3i piw =
            wrap<Vector3i>(state.pi, m_resolution, m_inv_resolution, m_wrap_mode);
        UInt32 idx =
            dr::fmadd( dr::fmadd(piw.z(), m_resolution.y(), piw.y()), m_resolution.x(), piw.x());

        Vector2f value = dr::gather<Vector2f>(m_extremum_grid, idx, active);

        ExtremumSegment segment(state.mint, t_next, value * m_scale);
        dr::masked(state.pi, active && !no_cross && (ti <= t_next)) += step;
        dr::masked(state.mint, active) = t_next;
        return segment;
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("data", m_extremum_grid, ParamFlags::NonDifferentiable);
        cb->put("resolution", m_resolution, ParamFlags::NonDifferentiable);
        Base::traverse(cb);
    }

    std::string to_string() const override {
        std::string wrap_str;
        switch (m_wrap_mode)
        {
        case dr::WrapMode::Clamp:
            wrap_str = "Clamp";
            break;
        case dr::WrapMode::Repeat:
            wrap_str = "Repeat";
            break;
        case dr::WrapMode::Mirror:
            wrap_str = "Mirror";
            break;
        default:
            break;
        }

        std::ostringstream oss;
        oss << "GridExtremum[" << std::endl
            << "  resolution = " << m_resolution << "," << std::endl
            << "  bbox = " << m_bbox << "," << std::endl
            << "  scale = " << m_scale << "," << std::endl
            << "  wrap_mode = " << wrap_str << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(GridExtremum)

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
            ScalarFloat min = volume->min();
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

private:
    /// Grid storing pre-computed local majorants
    FloatStorage m_extremum_grid;

    ScalarVector3i m_resolution;
    dr::divisor<int32_t> m_inv_resolution[3];
    dr::WrapMode m_wrap_mode;

    ScalarAffineTransform4f m_to_local;
};

MI_EXPORT_PLUGIN(GridExtremum)
NAMESPACE_END(mitsuba)
