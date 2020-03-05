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

// Forward declaration of specialized GridVolume
template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
class GridVolumeImpl;

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
class GridVolume final : public Volume<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Volume, m_world_to_local)
    MTS_IMPORT_TYPES()

    GridVolume(const Properties &props) : Base(props), m_props(props) {

        auto [metadata, raw_data] = read_binary_volume_data<Float>(props.string("filename"));
        m_metadata                = metadata;
        m_raw                     = props.bool_("raw", false);
        size_t size               = hprod(m_metadata.shape);
        // Apply spectral conversion if necessary
        if (is_spectral_v<Spectrum> && m_metadata.channel_count == 3 && !m_raw) {
            ScalarFloat *ptr = raw_data.get();
            auto scaled_data = std::unique_ptr<ScalarFloat[]>(new ScalarFloat[size * 4]);
            ScalarFloat *scaled_data_ptr = scaled_data.get();
            double mean = 0.0;
            ScalarFloat max = 0.0;
            for (size_t i = 0; i < size; ++i) {
                ScalarColor3f rgb = load_unaligned<ScalarColor3f>(ptr);
                // TODO: Make this scaling optional if the RGB values are between 0 and 1
                ScalarFloat scale = hmax(rgb) * 2.f;
                ScalarColor3f rgb_norm = rgb / std::max((ScalarFloat) 1e-8, scale);
                ScalarVector3f coeff = srgb_model_fetch(rgb_norm);
                mean += (double) (srgb_model_mean(coeff) * scale);
                max = std::max(max, scale);
                store_unaligned(scaled_data_ptr, concat(coeff, scale));
                ptr += 3;
                scaled_data_ptr += 4;
            }
            m_metadata.mean = mean;
            m_metadata.max = max;
            m_data = DynamicBuffer<Float>::copy(scaled_data.get(), size * 4);
        } else {
            m_data = DynamicBuffer<Float>::copy(raw_data.get(), size * m_metadata.channel_count);
        }

        // Mark values which are only used in the implementation class as queried
        props.mark_queried("use_grid_bbox");
        props.mark_queried("max_value");
    }

    Mask is_inside(const Interaction3f & /* it */, Mask /*active*/) const override {
       return true; // dummy implementation
    }

    template <uint32_t Channels, bool Raw> using Impl = GridVolumeImpl<Float, Spectrum, Channels, Raw>;

    /**
     * Recursively expand into an implementation specialized to the actual loaded grid.
     */
    std::vector<ref<Object>> expand() const override {
        ref<Object> result;
        switch (m_metadata.channel_count) {
            case 1:
                result = m_raw ? (Object *) new Impl<1, true>(m_props, m_metadata, m_data)
                               : (Object *) new Impl<1, false>(m_props, m_metadata, m_data);
                break;
            case 3:
                result = m_raw ? (Object *) new Impl<3, true>(m_props, m_metadata, m_data)
                               : (Object *) new Impl<3, false>(m_props, m_metadata, m_data);
                break;
            default:
                Throw("Unsupported channel count: %d (expected 1 or 3)", m_metadata.channel_count);
        }
        return { result };
    }

    MTS_DECLARE_CLASS()
protected:
    bool m_raw;
    DynamicBuffer<Float> m_data;
    VolumeMetadata m_metadata;
    Properties m_props;
};

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
class GridVolumeImpl final : public Volume<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Volume, is_inside, update_bbox, m_world_to_local)
    MTS_IMPORT_TYPES()

    GridVolumeImpl(const Properties &props, const VolumeMetadata &meta,
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

    UnpolarizedSpectrum eval(const Interaction3f &it, Mask active) const override {
        ENOKI_MARK_USED(it);
        ENOKI_MARK_USED(active);

        if constexpr (Channels == 3 && is_spectral_v<Spectrum> && Raw) {
            Throw("The GridVolume texture %s was queried for a spectrum, but texture conversion "
                  "into spectra was explicitly disabled! (raw=true)",
                  to_string());
        } else if constexpr (Channels != 3 && Channels != 1) {
            Throw("The GridVolume texture %s was queried for a spectrum, but has a number of channels "
                  "which is not 1 or 3",
                  to_string());
        } else {
            auto result = eval_impl<false>(it, active);

            if constexpr (Channels == 3 && is_monochromatic_v<Spectrum>)
                return mitsuba::luminance(Color3f(result));
            else if constexpr (Channels == 1)
                return result.x();
            else
                return result;
        }
    }

    Float eval_1(const Interaction3f &it, Mask active = true) const override {
        ENOKI_MARK_USED(it);
        ENOKI_MARK_USED(active);

        if constexpr (Channels == 3 && is_spectral_v<Spectrum> && !Raw) {
            Throw("eval_1(): The GridVolume texture %s was queried for a scalar value, but texture "
                  "conversion into spectra was requested! (raw=false)",
                  to_string());
        } else {
            auto result = eval_impl<false>(it, active);
            if constexpr (Channels == 3)
                return mitsuba::luminance(Color3f(result));
            else
                return hmean(result);
        }
    }


    Vector3f eval_3(const Interaction3f &it, Mask active = true) const override {
        ENOKI_MARK_USED(it);
        ENOKI_MARK_USED(active);

        if constexpr (Channels != 3) {
            Throw("eval_3(): The GridVolume texture %s was queried for a 3D vector, but it has "
                  "only a single channel!", to_string());
        } else if constexpr (is_spectral_v<Spectrum> && !Raw) {
            Throw("eval_3(): The GridVolume texture %s was queried for a 3D vector, but texture "
                  "conversion into spectra was requested! (raw=false)",
                  to_string());
        } else {
            return eval_impl<false>(it, active);
        }
    }


    std::pair<UnpolarizedSpectrum, Vector3f> eval_gradient(const Interaction3f &it,
                                                           Mask active) const override {
        ENOKI_MARK_USED(it);
        ENOKI_MARK_USED(active);

        if constexpr (Channels != 1)
            Throw("eval_gradient() is currently only supported for single channel grids!", to_string());
        else {
            auto [result, gradient] = eval_impl<true>(it, active);
            return { result.x(), gradient };
        }
    }


    template <bool with_gradient>
    MTS_INLINE auto eval_impl(const Interaction3f &it, Mask active) const {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        using StorageType = Array<Float, Channels>;
        constexpr bool uses_srgb_model = is_spectral_v<Spectrum> && !Raw && Channels == 3;
        using ResultType = std::conditional_t<uses_srgb_model, UnpolarizedSpectrum, StorageType>;

        auto p = m_world_to_local * it.p;
        active &= all((p >= 0) && (p <= 1));

        if constexpr (with_gradient) {
            if (none_or<false>(active))
                return std::make_pair(zero<ResultType>(), zero<Vector3f>());
            auto [result, gradient] = interpolate<true>(p, it.wavelengths, active);
            return std::make_pair(select(active, result, zero<ResultType>()),
                                  select(active, gradient, zero<Vector3f>()));
        } else {
            if (none_or<false>(active))
                return zero<ResultType>();
            ResultType result = interpolate<false>(p, it.wavelengths, active);
            return select(active, result, zero<ResultType>());
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
        constexpr bool uses_srgb_model = is_spectral_v<Spectrum> && !Raw && Channels == 3;
        using StorageType = std::conditional_t<uses_srgb_model, Array<Float, 4>, Array<Float, Channels>>;
        using ResultType = std::conditional_t<uses_srgb_model, UnpolarizedSpectrum, StorageType>;

        // TODO: in the autodiff case, make sure these do not trigger a recompilation
        const uint32_t nx = m_metadata.shape.x();
        const uint32_t ny = m_metadata.shape.y();
        const uint32_t nz = m_metadata.shape.z();
        const uint32_t z_offset = nx * ny;

        Point3f max_coordinates(nx - 1.f, ny - 1.f, nz - 1.f);
        p *= max_coordinates;

        // Integer part (clamped to include the upper bound)
        Index3 pi  = enoki::floor2int<Index3>(p);
        pi[active] = clamp(pi, 0, max_coordinates - 1);

        // Fractional part
        Point3f f = p - Point3f(pi), rf = 1.f - f;
        active &= all(pi >= 0u && (pi + 1u) < Index3(nx, ny, nz));

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

        ResultType v000, v001, v010, v011, v100, v101, v110, v111;
        Float scale = 1.f;
        if constexpr (uses_srgb_model) {
            v000 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d000), wavelengths);
            v001 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d001), wavelengths);
            v010 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d010), wavelengths);
            v011 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d011), wavelengths);
            v100 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d100), wavelengths);
            v101 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d101), wavelengths);
            v110 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d110), wavelengths);
            v111 = srgb_model_eval<UnpolarizedSpectrum>(head<3>(d111), wavelengths);

            Float f00 = fmadd(d000.w(), rf.x(), d001.w() * f.x()),
                  f01 = fmadd(d010.w(), rf.x(), d011.w() * f.x()),
                  f10 = fmadd(d100.w(), rf.x(), d101.w() * f.x()),
                  f11 = fmadd(d110.w(), rf.x(), d111.w() * f.x());
            Float f0  = fmadd(f00, rf.y(), f01 * f.y()),
                  f1  = fmadd(f10, rf.y(), f11 * f.y());
            scale = fmadd(f0, rf.z(), f1 * f.z());
        } else {
            v000 = d000; v001 = d001; v010 = d010; v011 = d011;
            v100 = d100; v101 = d101; v110 = d110; v111 = d111;
            ENOKI_MARK_USED(scale);
            ENOKI_MARK_USED(wavelengths);
        }

        // Trilinear interpolation
        ResultType v00 = fmadd(v000, rf.x(), v001 * f.x()),
                   v01 = fmadd(v010, rf.x(), v011 * f.x()),
                   v10 = fmadd(v100, rf.x(), v101 * f.x()),
                   v11 = fmadd(v110, rf.x(), v111 * f.x());
        ResultType v0  = fmadd(v00, rf.y(), v01 * f.y()),
                   v1  = fmadd(v10, rf.y(), v11 * f.y());
        ResultType result = fmadd(v0, rf.z(), v1 * f.z());

        if constexpr (uses_srgb_model)
            result *= scale;

        if constexpr (with_gradient) {
            if constexpr (!is_monochromatic_v<Spectrum>)
                NotImplementedError("eval_gradient with multichannel GridVolume texture");

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
                Throw("Unsupported GridVolume data size update: %d -> %d. Expected %d or %d "
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
        oss << "GridVolume[" << std::endl
            << "  world_to_local = " << m_world_to_local << "," << std::endl
            << "  dimensions = " << m_metadata.shape << "," << std::endl
            << "  mean = " << m_metadata.mean << "," << std::endl
            << "  max = " << m_metadata.max << "," << std::endl
            << "  channels = " << m_metadata.channel_count << std::endl
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

MTS_IMPLEMENT_CLASS_VARIANT(GridVolume, Volume)
MTS_EXPORT_PLUGIN(GridVolume, "GridVolume texture")

NAMESPACE_BEGIN(detail)
template <uint32_t Channels, bool Raw>
constexpr const char * gridvolume_class_name() {
    if constexpr (!Raw) {
        if constexpr (Channels == 1)
            return "GridVolumeImpl_1_0";
        else
            return "GridVolumeImpl_3_0";
    } else {
        if constexpr (Channels == 1)
            return "GridVolumeImpl_1_1";
        else
            return "GridVolumeImpl_3_1";
    }
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
Class *GridVolumeImpl<Float, Spectrum, Channels, Raw>::m_class
    = new Class(detail::gridvolume_class_name<Channels, Raw>(), "Volume",
                ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
const Class* GridVolumeImpl<Float, Spectrum, Channels, Raw>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
