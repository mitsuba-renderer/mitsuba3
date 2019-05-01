#include <fstream>
#include <sstream>

#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/texture3d.h>

#include "volume_data.h"

NAMESPACE_BEGIN(mitsuba)

/**
 * Interpolated 3D grid texture of floating point values.
 * Values can be read from the Mitsuba 1 binary volume format, or a simple
 * comma-separated string (for testing).
 *
 * The data must be ordered so that the following C-style (row-major) indexing
 * operation makes sense after the file has been mapped into memory:
 *     data[((zpos*yres + ypos)*xres + xpos)*channels + chan]}
 *     where (xpos, ypos, zpos, chan) denotes the lookup location.
 */
class Grid3D final : public Grid3DBase {
public:
    MTS_AUTODIFF_GETTER(data, m_data, m_data_d)

public:
    explicit Grid3D(const Properties &props) : Grid3DBase(props) {
        VolumeMetadata meta;
        if (props.has_property("filename")) {
            if (props.has_property("side") || props.has_property("nx") ||
                props.has_property("ny") || props.has_property("nz"))
                Throw("Grid dimensions are already specified in the binary volume "
                      "file, cannot override them with properties side, nx, ny or "
                      "nz.");

            meta = read_binary_volume_data<1>(props.string("filename"), &m_data);
        } else {
            meta = parse_string_grid(props, &m_data);
        }

        set_metadata(meta, props.bool_("use_grid_bbox", false));
        if (props.has_property("max_value")) {
            m_fixed_max    = true;
            m_metadata.max = props.float_("max_value");
        }

#if defined(MTS_ENABLE_AUTODIFF)
        // Copy parsed data over to the GPU
        m_data_d = CUDAArray<Float>::copy(m_data.data(), m_size);
#endif
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override { dp.put(this, "data", m_data_d); }

    /// Note: this assumes that the size has not changed.
    void parameters_changed() override {
        Grid3DBase::parameters_changed();

        m_metadata.mean = (double) hsum(detach(m_data_d))[0] / (double) m_size;
        if (!m_fixed_max)
            m_metadata.max = hmax(m_data_d)[0];
    }

    size_t data_size() const override { return m_data_d.size(); }
#endif

    /**
     * Taking a 3D point in [0, 1)^3, estimates the grid's value at that
     * point using trilinear interpolation.
     *
     * The passed `active` mask must disable lanes that are not within the
     * domain.
     */
    template <bool with_gradient, typename Point3, typename Value = value_t<Point3>>
    MTS_INLINE auto trilinear_interpolation(Point3 p, mask_t<Value> active) const {
        using Index    = uint32_array_t<Value>;
        using Index3   = uint32_array_t<Point3>;
        using Spectrum = mitsuba::Spectrum<Value>;
        using Vector3  = Vector<Value, 3>;

        auto max_coordinates = Point3(nx<Value>() - 1.f, ny<Value>() - 1.f, nz<Value>() - 1.f);
        p *= max_coordinates;

        // Integer part (clamped to include the upper bound)
        Index3 pi  = enoki::floor2int<Index3>(p);
        pi[active] = clamp(pi, 0, max_coordinates - 1);

        // Fractional part
        Point3 f = p - Point3(pi), rf = 1.f - f;
        active &= all(pi >= 0 && (pi + 1) < Index3(nx<Value>(), ny<Value>(), nz<Value>()));

        auto wgather = [&](const Index &index) {
            if constexpr (!is_diff_array_v<Index>) {
                return gather<Value>(m_data.data(), index, active);
            }
#if defined(MTS_ENABLE_AUTODIFF)
            else {
                return gather<Value>(m_data_d, index, active);
            }
#endif
        };

        // (z * ny + y) * nx + x
        Index index = fmadd(fmadd(pi.z(), ny<Value>(), pi.y()), nx<Value>(), pi.x());

        Value d000 = wgather(index), d001 = wgather(index + 1), d010 = wgather(index + nx<Value>()),
              d011 = wgather(index + nx<Value>() + 1),

              d100 = wgather(index + z_offset<Value>()),
              d101 = wgather(index + z_offset<Value>() + 1),
              d110 = wgather(index + z_offset<Value>() + nx<Value>()),
              d111 = wgather(index + z_offset<Value>() + nx<Value>() + 1);

        Value d00 = fmadd(d000, rf.x(), d001 * f.x()), d01 = fmadd(d010, rf.x(), d011 * f.x()),
              d10 = fmadd(d100, rf.x(), d101 * f.x()), d11 = fmadd(d110, rf.x(), d111 * f.x());

        Value d0 = fmadd(d00, rf.y(), d01 * f.y()), d1 = fmadd(d10, rf.y(), d11 * f.y());

        Value d = fmadd(d0, rf.z(), d1 * f.z());

        if constexpr (!with_gradient)
            return Spectrum(d);
        else {
            Value gx0 = fmadd(d001 - d000, rf.y(), (d011 - d010) * f.y()),
                  gx1 = fmadd(d101 - d100, rf.y(), (d111 - d110) * f.y()),
                  gy0 = fmadd(d010 - d000, rf.x(), (d011 - d001) * f.x()),
                  gy1 = fmadd(d110 - d100, rf.x(), (d111 - d101) * f.x()),
                  gz0 = fmadd(d100 - d000, rf.x(), (d101 - d001) * f.x()),
                  gz1 = fmadd(d110 - d010, rf.x(), (d111 - d011) * f.x());

            // Smaller grid cells means variation is faster (-> larger gradient)
            Vector3 gradient(fmadd(gx0, rf.z(), gx1 * f.z()) * (nx<Value>() - 1),
                             fmadd(gy0, rf.z(), gy1 * f.z()) * (ny<Value>() - 1),
                             fmadd(gz0, rf.y(), gz1 * f.y()) * (nz<Value>() - 1));
            return std::pair(Spectrum(d), gradient);
        }
    }

    template <bool with_gradient, typename Interaction,
              typename Value = typename Interaction::Value>
    MTS_INLINE auto eval_impl(const Interaction &it, mask_t<Value> active) const {
        using Spectrum = mitsuba::Spectrum<Value>;
        using Vector3  = Vector<Value, 3>;

        auto p = m_world_to_local * it.p;
        active &= all((p >= 0) && (p <= 1));

        if constexpr (with_gradient) {
            if (none_or<false>(active))
                return std::make_pair(zero<Spectrum>(), zero<Vector3>());
            auto [result, gradient] = trilinear_interpolation<true>(p, active);
            return std::make_pair(select(active, result, zero<Spectrum>()),
                                  select(active, gradient, zero<Vector3>()));
        } else {
            if (none_or<false>(active))
                return zero<Spectrum>();
            Spectrum result = trilinear_interpolation<false>(p, active);
            return select(active, result, zero<Spectrum>());
        }
    }

    MTS_IMPLEMENT_TEXTURE_3D()
    MTS_DECLARE_CLASS()

protected:
    FloatX m_data;
    bool m_fixed_max = false;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_data_d;
#endif
};

MTS_IMPLEMENT_CLASS(Grid3DBase, Texture3D)
MTS_IMPLEMENT_CLASS(Grid3D, Grid3DBase)
MTS_EXPORT_PLUGIN(Grid3D, "Grid 3D texture with interpolation")

NAMESPACE_END(mitsuba)
