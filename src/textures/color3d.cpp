#include <fstream>
#include <sstream>

#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/render/texture3d.h>

#include "volume_data.h"

NAMESPACE_BEGIN(mitsuba)

/**
 * Interpolated 3D grid texture of color values.
 * See the \ref Grid3D plugin for a description of the data format.
 *
 * This plugin loads RGB data from a binary file. Spectral upsampling is applied
 * at loading time to convert RGB values to spectra that can be used in the renderer.
 *
 * Unlike \ref Grid3D, loading from string values is not supported.
 */
class Color3D final : public Grid3DBase {
public:
    MTS_AUTODIFF_GETTER(data, m_data, m_data_d)

public:
    explicit Color3D(const Properties &props) : Grid3DBase(props) {
        // Read 3-channel RGB grid from binary file
        auto meta = read_binary_volume_data<3>(props.string("filename"), &m_data);
        set_metadata(meta, props.bool_("use_grid_bbox", false));

#if defined(MTS_ENABLE_AUTODIFF)
        // Copy parsed data over to the GPU
        for (int i = 0; i < 3; ++i)
            m_data_d[i] = CUDAArray<Float>::copy(m_data[i].data(), m_size);
#endif
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override { dp.put(this, "data", m_data_d); }

    void parameters_changed() override {
        Grid3DBase::parameters_changed();

        m_metadata.mean = (double) hsum(hsum(detach(m_data_d)))[0] / (double) (m_size * 3);
        m_metadata.max  = hmax(hmax(m_data_d))[0];
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
    template <typename Point3, typename Value = value_t<Point3>,
              typename Spectrum = mitsuba::Spectrum<Value>>
    MTS_INLINE auto trilinear_interpolation(Point3 p, const Spectrum &wavelengths,
                                            mask_t<Value> active) const {
        using Index   = uint32_array_t<Value>;
        using Index3  = uint32_array_t<Point3>;
        using Vector3 = Vector<Value, 3>;

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
                return gather<Vector3>(m_data.data(), index, active);
            }
#if defined(MTS_ENABLE_AUTODIFF)
            else {
                return gather<Vector3>(m_data_d, index, active);
            }
#endif
        };

        // (z * ny + y) * nx + x
        Index index = fmadd(fmadd(pi.z(), ny<Value>(), pi.y()), nx<Value>(), pi.x());

        Vector3 d000 = wgather(index), d001 = wgather(index + 1),
                d010 = wgather(index + nx<Value>()), d011 = wgather(index + nx<Value>() + 1),

                d100  = wgather(index + z_offset<Value>()),
                d101  = wgather(index + z_offset<Value>() + 1),
                d110  = wgather(index + z_offset<Value>() + nx<Value>()),
                d111  = wgather(index + z_offset<Value>() + nx<Value>() + 1);
        Spectrum v000 = srgb_model_eval(d000, wavelengths),
                 v001 = srgb_model_eval(d001, wavelengths),
                 v010 = srgb_model_eval(d010, wavelengths),
                 v011 = srgb_model_eval(d011, wavelengths),

                 v100 = srgb_model_eval(d100, wavelengths),
                 v101 = srgb_model_eval(d101, wavelengths),
                 v110 = srgb_model_eval(d110, wavelengths),
                 v111 = srgb_model_eval(d111, wavelengths);

        Spectrum v00 = fmadd(v000, rf.x(), v001 * f.x()), v01 = fmadd(v010, rf.x(), v011 * f.x()),
                 v10 = fmadd(v100, rf.x(), v101 * f.x()), v11 = fmadd(v110, rf.x(), v111 * f.x());

        Spectrum v0 = fmadd(v00, rf.y(), v01 * f.y()), v1 = fmadd(v10, rf.y(), v11 * f.y());

        return fmadd(v0, rf.z(), v1 * f.z());
    }

    template <bool with_gradient, typename Interaction,
              typename Value = typename Interaction::Value>
    MTS_INLINE auto eval_impl(const Interaction &it, mask_t<Value> active) const {
        using Spectrum = mitsuba::Spectrum<Value>;
        using Vector3  = Vector<Value, 3>;

        auto p = m_world_to_local * it.p;
        active &= all((p >= 0) && (p <= 1));

        if constexpr (with_gradient) {
            NotImplementedError("eval_gradient");
            return std::make_pair(zero<Spectrum>(), zero<Vector3>());
        } else {
            if (none_or<false>(active))
                return zero<Spectrum>();
            Spectrum result = trilinear_interpolation(p, it.wavelengths, active);
            return select(active, result, zero<Spectrum>());
        }
    }

    MTS_IMPLEMENT_TEXTURE_3D()
    MTS_DECLARE_CLASS()

protected:
    Vector3fX m_data;
#if defined(MTS_ENABLE_AUTODIFF)
    Vector3fD m_data_d;
#endif
};

MTS_IMPLEMENT_CLASS(Grid3DBase, Texture3D)
MTS_IMPLEMENT_CLASS(Color3D, Grid3DBase)
MTS_EXPORT_PLUGIN(Color3D, "Color 3D texture with interpolation")

NAMESPACE_END(mitsuba)
