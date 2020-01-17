#include <enoki/stl.h>

#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume_texture.h>

#include "volume_data.h"

NAMESPACE_BEGIN(mitsuba)

// Forward declaration of specialized grid3d
template <typename Float, typename Spectrum, uint32_t Channels>
class Grid3DImpl;

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
class Grid3D final : public Texture3D<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Texture3D, m_world_to_local)
    MTS_IMPORT_TYPES()

    explicit Grid3D(const Properties &props) : Base(props), m_props(props) {

        auto [metadata, raw_data] = read_binary_volume_data<Float>(props.string("filename"));
        m_metadata = metadata;

        bool raw = props.bool_("raw", false);

        size_t size = hprod(m_metadata.shape);
        m_data = DynamicBuffer<Float>::copy(raw_data.get(), size * m_metadata.channel_count);

        // Apply spectral conversion if necessary
        // if (is_spectral_v<Spectrum> && m_metadata.channels == 3 && !raw) {
            //
        // }

        // Mark values which are only used in the implementation class as queried
        props.mark_queried("use_grid_bbox");
        props.mark_queried("max_value");
    }

    template <uint32_t Channels>
    using Impl = Grid3DImpl<Float, Spectrum, Channels>;

    /**
     * Recursively expand into an implementation specialized to the actual loaded grid.
     */
    std::vector<ref<Object>> expand() const override {
        ref<Object> result;
        switch (m_metadata.channel_count) {
            case 1:
                result = (Object *) new Impl<1>(m_props, m_metadata, m_data);
                break;
            case 3:
                result = (Object *) new Impl<3>(m_props, m_metadata, m_data);
                break;
            default:
                Throw("Unsupported channel count: %d (expected 1 or 3)", m_metadata.channel_count);
        }
        return { result };
    }

    MTS_DECLARE_CLASS()
protected:
    DynamicBuffer<Float> m_data;
    VolumeMetadata m_metadata;
    Properties m_props;
};

template <typename Float, typename Spectrum, uint32_t Channels>
class Grid3DImpl final : public Texture3D<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Texture3D, is_inside, update_bbox, m_world_to_local)
    MTS_IMPORT_TYPES()

    Grid3DImpl(const Properties &props, const VolumeMetadata &meta,
               const DynamicBuffer<Float> &data)
        : Base(props) {

        m_data     = data;
        m_metadata = meta;
        m_size     = hprod(m_metadata.shape);
        if (props.bool_("use_grid_bbox", false)) {
            m_world_to_local = m_metadata.transform * m_world_to_local;
            update_bbox();
        }

        if (props.has_property("max_value")) {
            m_fixed_max    = true;
            m_metadata.max = props.float_("max_value");
        }
    }

    Spectrum eval(const Interaction3f &it, Mask active) const override {
        return eval_impl<false>(it, active);
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
            auto [result, gradient] = interpolate<true>(p, it.wavelengths, active);
            return std::make_pair(select(active, result, zero<Spectrum>()),
                                  select(active, gradient, zero<Vector3f>()));
        } else {
            if (none_or<false>(active))
                return zero<Spectrum>();
            Spectrum result = interpolate<false>(p, it.wavelengths, active);
            return select(active, result, zero<Spectrum>());
        }
    }

    Mask is_inside(const Interaction3f &it, Mask /*active*/) const override {
        auto p = m_world_to_local * it.p;
        return all((p >= 0) && (p <= 1));
    }

    /**
     * Taking a 3D point in [0, 1)^3, estimates the grid's value at that
     * point using trilinear interpolation.
     *
     * The passed `active` mask must disable lanes that are not within the
     * domain.
     */
    template <bool with_gradient>
    MTS_INLINE auto interpolate(Point3f p, const Wavelength &wavelengths,
                                            Mask active) const {
        using Index   = uint32_array_t<Float>;
        using Index3  = uint32_array_t<Point3f>;
        // using StorageType = std::conditional_t<Channels == 1, Float, Color3f>;
        using StorageType = Array<Float, Channels>;


        // TODO: in the autodiff case, make sure these do not trigger a recompilation
        const size_t nx = m_metadata.shape.x();
        const size_t ny = m_metadata.shape.y();
        const size_t nz = m_metadata.shape.z();
        const size_t z_offset = nx * ny;

        Point3f max_coordinates(nx - 1.f, ny - 1.f, nz - 1.f);
        p *= max_coordinates;

        // Integer part (clamped to include the upper bound)
        Index3 pi  = enoki::floor2int<Index3>(p);
        pi[active] = clamp(pi, 0, max_coordinates - 1);

        // Fractional part
        Point3f f = p - Point3f(pi), rf = 1.f - f;
        active &= all(pi >= 0 && (pi + 1) < Index3(nx, ny, nz));

        // (z * ny + y) * nx + x
        Index index = fmadd(fmadd(pi.z(), ny, pi.y()), nx, pi.x());

        // Load 8 grid positions to perform trilinear interpolation
        auto raw_data = m_data.data();
        auto d000 = gather<StorageType>(raw_data, index, active),
             d001 = gather<StorageType>(raw_data, index + 1, active),
             d010 = gather<StorageType>(raw_data, index + nx, active),
             d011 = gather<StorageType>(raw_data, index + nx + 1, active),
             d100 = gather<StorageType>(raw_data, index + z_offset, active),
             d101 = gather<StorageType>(raw_data, index + z_offset + 1, active),
             d110 = gather<StorageType>(raw_data, index + z_offset + nx, active),
             d111 = gather<StorageType>(raw_data, index + z_offset + nx + 1, active);

        Spectrum  v000, v001, v010, v011, v100, v101, v110, v111;
        if constexpr (Channels == 3) {
            v000 = srgb_model_eval<UnpolarizedSpectrum>(d000, wavelengths);
            v001 = srgb_model_eval<UnpolarizedSpectrum>(d001, wavelengths);
            v010 = srgb_model_eval<UnpolarizedSpectrum>(d010, wavelengths);
            v011 = srgb_model_eval<UnpolarizedSpectrum>(d011, wavelengths);

            v100 = srgb_model_eval<UnpolarizedSpectrum>(d100, wavelengths);
            v101 = srgb_model_eval<UnpolarizedSpectrum>(d101, wavelengths);
            v110 = srgb_model_eval<UnpolarizedSpectrum>(d110, wavelengths);
            v111 = srgb_model_eval<UnpolarizedSpectrum>(d111, wavelengths);
        } else if constexpr (Channels == 1) {
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
            if constexpr (!is_monochromatic_v<Spectrum>)
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

    ScalarFloat max() const override { return m_metadata.max; }
    ScalarVector3i resolution() const override { return m_metadata.shape; };
    size_t data_size() const { return m_data.size(); }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("data", m_data);
        callback->put_parameter("size", m_size);
        Base::traverse(callback);
    }

    void parameters_changed() override {
        size_t new_size = data_size();
        if (m_size != new_size) {
            // Only support a special case: resolution doubling along all axes
            if (new_size != m_size * 8)
                Throw("Unsupported Grid3D data size update: %d -> %d. Expected %d or %d "
                      "(doubling "
                      "the resolution).",
                      m_size, new_size, m_size, m_size * 8);
            m_metadata.shape *= 2;
            m_size = new_size;
        }

        auto sum = hsum(hsum(detach(m_data)));
        m_metadata.mean = (double) enoki::slice(sum, 0) / (double) (m_size * 3);
        if (!m_fixed_max) {
            auto maximum = hmax(hmax(m_data));
            m_metadata.max = slice(maximum, 0);
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Grid3D[" << std::endl
            << "  world_to_local = " << m_world_to_local << "," << std::endl
            << "  dimensions = " << m_metadata.shape << "," << std::endl
            << "  mean = " << m_metadata.mean << "," << std::endl
            << "  max = " << m_metadata.max << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    DynamicBuffer<Float> m_data;
    bool m_fixed_max = false;
    VolumeMetadata m_metadata;
    size_t m_size;
};

MTS_IMPLEMENT_CLASS_VARIANT(Grid3D, Texture3D)
MTS_EXPORT_PLUGIN(Grid3D, "Grid 3D texture with interpolation")

template <typename Float, typename Spectrum, uint32_t Channels>
const Class *Grid3DImpl<Float, Spectrum, Channels>::class_() const {
    return m_class;
}

template <typename Float, typename Spectrum, uint32_t Channels>
Class *Grid3DImpl<Float, Spectrum, Channels>::m_class = Grid3D<Float, Spectrum>::m_class;

NAMESPACE_END(mitsuba)
