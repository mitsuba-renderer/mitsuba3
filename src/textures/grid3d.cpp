#include <fstream>
#include <sstream>

#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/volume_texture.h>
#include <mitsuba/render/srgb.h>
#include "volume_data.h"

NAMESPACE_BEGIN(mitsuba)

/**
 * Interpolated 3D grid texture of scalar or color values.
 *
 * This plugin loads RGB data from a binary file. When appropriate,
 * spectral upsampling is applied at loading time to convert RGB values
 * to spectra that can be used in the renderer.
 *
 * Data layout:
 * The data must be ordered so that the following C-style (row-major) indexing
 * operation makes sense after the file has been mapped into memory:
 *     data[((zpos*yres + ypos)*xres + xpos)*channels + chan]}
 *     where (xpos, ypos, zpos, chan) denotes the lookup location.
 */
template <typename Float, typename Spectrum>
class Grid3D final : public Grid3DBase<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(Grid3D, Grid3DBase)
    MTS_USING_BASE(Grid3DBase, set_metadata, m_metadata, m_size, m_world_to_local)
    MTS_IMPORT_TYPES()

    static constexpr int m_channel_count = texture_channels_v<Spectrum>;
    using DynamicBuffer = DynamicBuffer<Float>;
    // TODO: linearize data storage
    using DataBuffer    = Vector<DynamicBuffer, m_channel_count>;

    Grid3D(const Properties &props) : Base(props) {
        // Read volume data from the binary file, typically 1 or 3 channels
        auto meta =
            read_binary_volume_data<Float, m_channel_count>(props.string("filename"), &m_data);

        set_metadata(meta, props.bool_("use_grid_bbox", false));
        if (props.has_property("max_value")) {
            m_fixed_max    = true;
            m_metadata.max = props.float_("max_value");
        }

#if defined(MTS_ENABLE_AUTODIFF)
        // Copy parsed data over to the GPU
        if constexpr(is_monochrome_v<Spectrum>) {
            m_data_d = CUDAArray<Float>::copy(m_data.data(), m_size);
        } else {
            for (int i = 0; i < 3; ++i)
                m_data_d[i] = CUDAArray<Float>::copy(m_data[i].data(), m_size);
        }
#endif
    }

    Spectrum eval(const Interaction3f &it, Mask active) const override {
        return eval_impl<false>(it, active);
    }

    Vector3f eval3(const Interaction3f & /*it*/, Mask  /*active*/) const override {
        NotImplementedError("eval3");
    }

    Float eval1(const Interaction3f & /*it*/, Mask  /*active*/) const override {
        NotImplementedError("eval1");
    }

    std::pair<Spectrum, Vector3f> eval_gradient(const Interaction3f &it,
                                                Mask active) const override {
        return eval_impl<true>(it, active);
    }

    template <bool with_gradient>
    MTS_INLINE auto eval_impl(const Interaction3f &it, Mask active) const {

        auto p = m_world_to_local * it.p;
        active &= all((p >= 0) && (p <= 1));

        if constexpr (with_gradient) {
            if (none_or<false>(active))
                return std::make_pair(zero<Spectrum>(), zero<Vector3f>());
            auto [result, gradient] = trilinear_interpolation<true>(p, it.wavelengths, active);
            return std::make_pair(select(active, result, zero<Spectrum>()),
                                  select(active, gradient, zero<Vector3f>()));
        } else {
            if (none_or<false>(active))
                return zero<Spectrum>();
            Spectrum result = trilinear_interpolation<false>(p, it.wavelengths, active);
            return select(active, result, zero<Spectrum>());
        }
    }

    /**
     * Taking a 3D point in [0, 1)^3, estimates the grid's value at that
     * point using trilinear interpolation.
     *
     * The passed `active` mask must disable lanes that are not within the
     * domain.
     */
    template <bool with_gradient>
    MTS_INLINE auto trilinear_interpolation(Point3f p, const Wavelength &wavelengths,
                                            Mask active) const {
        using Index   = uint32_array_t<Float>;
        using Index3  = uint32_array_t<Point3f>;

        // TODO: in the autodiff case, make sure these do no trigger a recompilation
        const size_t nx = m_metadata.shape.x();
        const size_t ny = m_metadata.shape.y();
        const size_t nz = m_metadata.shape.z();
        const size_t z_offset = nx * ny;

        Point3f max_coordinates(
            nx - 1.f, ny - 1.f, nz - 1.f);
        p *= max_coordinates;

        // Integer part (clamped to include the upper bound)
        Index3 pi  = enoki::floor2int<Index3>(p);
        pi[active] = clamp(pi, 0, max_coordinates - 1);

        // Fractional part
        Point3f f = p - Point3f(pi), rf = 1.f - f;
        active &= all(pi >= 0 && (pi + 1) < Index3(nx, ny, nz));

        auto wgather = [&](const Index &index) {
            using VectorNf = Array<Float, m_channel_count>;
            // TODO: unify this
            if constexpr (!is_diff_array_v<Index>) {
                return gather<VectorNf>(m_data.data(), index, active);
            }
#if defined(MTS_ENABLE_AUTODIFF)
            else {
                return gather<VectorNf>(m_data_d, index, active);
            }
#endif
        };

        // (z * ny + y) * nx + x
        Index index = fmadd(fmadd(pi.z(), ny, pi.y()), nx, pi.x());

        // Load 8 grid positions to perform trilinear interpolation
        auto d000 = wgather(index),
             d001 = wgather(index + 1),
             d010 = wgather(index + nx),
             d011 = wgather(index + nx + 1),
             d100 = wgather(index + z_offset),
             d101 = wgather(index + z_offset + 1),
             d110 = wgather(index + z_offset + nx),
             d111 = wgather(index + z_offset + nx + 1);

        Spectrum  v000, v001, v010, v011, v100, v101, v110, v111;
        if constexpr (is_spectral_v<Spectrum>) {
            v000 = srgb_model_eval<Spectrum>(d000, wavelengths);
            v001 = srgb_model_eval<Spectrum>(d001, wavelengths);
            v010 = srgb_model_eval<Spectrum>(d010, wavelengths);
            v011 = srgb_model_eval<Spectrum>(d011, wavelengths);

            v100 = srgb_model_eval<Spectrum>(d100, wavelengths);
            v101 = srgb_model_eval<Spectrum>(d101, wavelengths);
            v110 = srgb_model_eval<Spectrum>(d110, wavelengths);
            v111 = srgb_model_eval<Spectrum>(d111, wavelengths);
        } else if constexpr (is_monochrome_v<Spectrum>) {
            v000 = d000.x(); v001 = d001.x(); v010 = d010.x(); v011 = d011.x();
            v100 = d100.x(); v101 = d101.x(); v110 = d110.x(); v111 = d111.x();
        } else {
            // No conversion to do
            v000 = d000; v001 = d001; v010 = d010; v011 = d011;
            v100 = d100; v101 = d101; v110 = d110; v111 = d111;
        }

        // Trilinear interpolation
        Spectrum v00 = fmadd(v000, rf.x(), v001 * f.x()),
                 v01 = fmadd(v010, rf.x(), v011 * f.x()),
                 v10 = fmadd(v100, rf.x(), v101 * f.x()),
                 v11 = fmadd(v110, rf.x(), v111 * f.x());
        Spectrum v0  = fmadd(v00, rf.y(), v01 * f.y()),
                 v1  = fmadd(v10, rf.y(), v11 * f.y());
        Spectrum result = fmadd(v0, rf.z(), v1 * f.z());

        if constexpr (with_gradient) {
            if constexpr (!is_monochrome_v<Spectrum>)
                NotImplementedError("eval_gradient with multichannel Grid3D texture");

            Float gx0 = fmadd(d001 - d000, rf.y(), (d011 - d010) * f.y()).x(),
                  gx1 = fmadd(d101 - d100, rf.y(), (d111 - d110) * f.y()).x(),
                  gy0 = fmadd(d010 - d000, rf.x(), (d011 - d001) * f.x()).x(),
                  gy1 = fmadd(d110 - d100, rf.x(), (d111 - d101) * f.x()).x(),
                  gz0 = fmadd(d100 - d000, rf.x(), (d101 - d001) * f.x()).x(),
                  gz1 = fmadd(d110 - d010, rf.x(), (d111 - d011) * f.x()).x();

            // Smaller grid cells means variation is faster (-> larger gradient)
            Vector3f gradient(fmadd(gx0, rf.z(), gx1 * f.z()) * (nx - 1),
                              fmadd(gy0, rf.z(), gy1 * f.z()) * (ny - 1),
                              fmadd(gz0, rf.y(), gz1 * f.y()) * (nz - 1));
            return std::make_pair(result, gradient);
        } else {
            return result;
        }

    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "data", m_data);
    }

    void parameters_changed() override {
        Base::parameters_changed();

        auto acc = hsum(hsum(detach(m_data)))[0];
        m_metadata.mean = (double) acc / (double) (m_size * 3);
        if (!m_fixed_max)
            m_metadata.max  = hmax(hmax(m_data))[0];
    }

    size_t data_size() const override { return m_data.size(); }
#endif

protected:
    DataBuffer m_data;
    bool m_fixed_max = false;
};

MTS_IMPLEMENT_PLUGIN(Grid3D, "Grid 3D texture with interpolation")
NAMESPACE_END(mitsuba)
