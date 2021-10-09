#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>

#include <mitsuba/render/srgb.h>
#include <mitsuba/render/volume.h>
#include <mitsuba/render/volumegrid.h>
#include <enoki/dynamic.h>
#include <enoki/tensor.h>

NAMESPACE_BEGIN(mitsuba)

enum class FilterType { Nearest, Trilinear };
enum class WrapMode { Repeat, Mirror, Clamp };

/**!
.. _volume-gridvolume:

Grid-based volume data source (:monosp:`gridvolume`)
----------------------------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the volume to be loaded

 * - filter_type
   - |string|
   - Specifies how voxel values are interpolated. The following options are currently available:

     - ``trilinear`` (default): perform trilinear interpolation.

     - ``nearest``: disable interpolation. In this mode, the plugin
       performs nearest neighbor lookups of volume values.

 * - wrap_mode
   - |string|
   - Controls the behavior of volume evaluations that fall outside of the
     :math:`[0, 1]` range. The following options are currently available:

     - ``clamp`` (default): clamp coordinates to the edge of the volume.

     - ``repeat``: tile the volume infinitely.

     - ``mirror``: mirror the volume along its boundaries.

 * - raw
   - |bool|
   - Should the transformation to the stored color data (e.g. sRGB to linear,
     spectral upsampling) be disabled? You will want to enable this when working
     with non-color, 3-channel volume data. Currently, no plugin needs this option
     to be set to true (Default: false)

 * - to_world
   - |transform|
   - Specifies an optional 4x4 transformation matrix that will be applied to volume coordinates.



This class implements access to volume data stored on a 3D grid using a
simple binary exchange format (compatible with Mitsuba 0.6). When appropriate,
spectral upsampling is applied at loading time to convert RGB values to
spectra that can be used in the renderer.
We provide a small `helper utility <https://github.com/mitsuba-renderer/mitsuba2-vdb-converter>`_
to convert OpenVDB files to this format. The format uses a
little endian encoding and is specified as follows:

.. list-table:: Volume file format
   :widths: 8 30
   :header-rows: 1

   * - Position
     - Content
   * - Bytes 1-3
     - ASCII Bytes ’V’, ’O’, and ’L’
   * - Byte 4
     - File format version number (currently 3)
   * - Bytes 5-8
     - Encoding identified (32-bit integer). Currently, only a value of 1 is
       supported (float32-based representation)
   * - Bytes 9-12
     - Number of cells along the X axis (32 bit integer)
   * - Bytes 13-16
     - Number of cells along the Y axis (32 bit integer)
   * - Bytes 17-20
     - Number of cells along the Z axis (32 bit integer)
   * - Bytes 21-24
     - Number of channels (32 bit integer, supported values: 1, 3 or 6)
   * - Bytes 25-48
     - Axis-aligned bounding box of the data stored in single precision (order:
       xmin, ymin, zmin, xmax, ymax, zmax)
   * - Bytes 49-*
     - Binary data of the volume stored in the specified encoding. The data
       are ordered so that the following C-style indexing operation makes sense
       after the file has been loaded into memory:
       :code:`data[((zpos*yres + ypos)*xres + xpos)*channels + chan]`
       where (xpos, ypos, zpos, chan) denotes the lookup location.

*/


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
    MTS_IMPORT_BASE(Volume, m_to_local)
    MTS_IMPORT_TYPES(VolumeGrid)

    GridVolume(const Properties &props) : Base(props), m_props(props) {
        std::string filter_type = props.string("filter_type", "trilinear");
        if (filter_type == "nearest")
            m_filter_type = FilterType::Nearest;
        else if (filter_type == "trilinear")
            m_filter_type = FilterType::Trilinear;
        else
            Throw("Invalid filter type \"%s\", must be one of: \"nearest\" or "
                  "\"trilinear\"!", filter_type);

        std::string wrap_mode = props.string("wrap_mode", "clamp");
        if (wrap_mode == "repeat")
            m_wrap_mode = WrapMode::Repeat;
        else if (wrap_mode == "mirror")
            m_wrap_mode = WrapMode::Mirror;
        else if (wrap_mode == "clamp")
            m_wrap_mode = WrapMode::Clamp;
        else
            Throw("Invalid wrap mode \"%s\", must be one of: \"repeat\", "
                  "\"mirror\", or \"clamp\"!", wrap_mode);

        FileResolver* fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_volume_grid = new VolumeGrid(file_path);

        m_raw = props.bool_("raw", false);

        ScalarVector3i res = m_volume_grid->size();
        ScalarUInt32 size = ek::hprod(res);

        // Apply spectral conversion if necessary
        if (is_spectral_v<Spectrum> && m_volume_grid->channel_count() == 3 && !m_raw) {
            ScalarFloat *ptr = m_volume_grid->data();

            auto scaled_data = std::unique_ptr<ScalarFloat[]>(new ScalarFloat[size * 4]);
            ScalarFloat *scaled_data_ptr = scaled_data.get();
            ScalarFloat max = 0.0;
            for (ScalarUInt32 i = 0; i < size; ++i) {
                ScalarColor3f rgb = ek::load<ScalarColor3f>(ptr);
                // TODO: Make this scaling optional if the RGB values are between 0 and 1
                ScalarFloat scale = ek::hmax(rgb) * 2.f;
                ScalarColor3f rgb_norm = rgb / ek::max((ScalarFloat) 1e-8, scale);
                ScalarVector3f coeff = srgb_model_fetch(rgb_norm);
                max = ek::max(max, scale);
                ek::store(
                    scaled_data_ptr,
                    ek::concat(coeff, ek::Array<ScalarFloat, 1>(scale)));
                ptr += 3;
                scaled_data_ptr += 4;
            }
            m_max = (float) max;

            size_t shape[4] = { (size_t) res.z(), (size_t) res.y(), (size_t) res.x(), 4 };
            m_data = TensorXf(scaled_data.get(), 4, shape);
        } else {
            size_t shape[4] = { (size_t) res.z(), (size_t) res.y(), (size_t) res.x(), m_volume_grid->channel_count() };
            m_data = TensorXf(m_volume_grid->data(), 4, shape);
            m_max = m_volume_grid->max();
        }

        // Mark values which are only used in the implementation class as queried
        props.mark_queried("use_grid_bbox");
        props.mark_queried("max_value");
    }

    /**
     * Recursively expand into an implementation specialized to the actual loaded grid.
     */
    std::vector<ref<Object>> expand() const override {
        return { ref<Object>(expand_1()) };
    }

    MTS_DECLARE_CLASS()

protected:
    Object* expand_1() const {
        switch (m_volume_grid->channel_count()) {
            case 1:
                return expand_2<1>();
            case 3:
                return expand_2<3>();
            case 6:
                return expand_2<6>();
            default:
                Throw("Unsupported channel count: %d (expected 1, 3, or 6)",
                       m_volume_grid->channel_count());
        }
    }

    template <uint32_t Channels> Object* expand_2() const {
        return m_raw ? expand_3<Channels, true>() : expand_3<Channels, false>();
    }

    template <uint32_t Channels, bool Raw> Object* expand_3() const {
        return new GridVolumeImpl<Float, Spectrum, Channels, Raw>(
            m_props, m_data, m_max, m_volume_grid->bbox_transform(),
            m_filter_type, m_wrap_mode);
    }

protected:
    bool m_raw;
    TensorXf m_data;
    ref<VolumeGrid> m_volume_grid;
    Properties m_props;
    FilterType m_filter_type;
    WrapMode m_wrap_mode;
    ScalarFloat m_max;
};

template <typename Float, typename Spectrum, uint32_t Channels, bool Raw>
class GridVolumeImpl final : public Volume<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Volume, update_bbox, m_to_local, m_bbox)
    MTS_IMPORT_TYPES()

    GridVolumeImpl(const Properties &props,
                   const TensorXf &data,
                   ScalarFloat max,
                   const ScalarTransform4f &bbox_transform,
                   FilterType filter_type, WrapMode wrap_mode)
        : Base(props), m_data(data), m_max(max), m_filter_type(filter_type),
          m_wrap_mode(wrap_mode) {

        ScalarVector3i res = resolution();
        m_inv_resolution_x = ek::divisor<int32_t>(res.x());
        m_inv_resolution_y = ek::divisor<int32_t>(res.y());
        m_inv_resolution_z = ek::divisor<int32_t>(res.z());

        if (props.bool_("use_grid_bbox", false)) {
            m_to_local = bbox_transform * m_to_local;
            update_bbox();
        }

        if (props.has_property("max_value")) {
            m_fixed_max    = true;
            m_max = props.float_("max_value");
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
            auto result = eval_impl(it, active);

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
            auto result = eval_impl(it, active);
            if constexpr (Channels == 3)
                return mitsuba::luminance(Color3f(result));
            else
                return ek::hmean(result);
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
            return eval_impl(it, active);
        }
    }

    ek::Array<Float, 6> eval_6(const Interaction3f &it, Mask active = true) const override {
        ENOKI_MARK_USED(it);
        ENOKI_MARK_USED(active);

        if constexpr (Channels != 6) {
            Throw("eval_6(): The GridVolume texture %s was queried for a 6D vector,"
                  " but it has %s channel(s)", to_string(), Channels);
        } else {
            return eval_impl(it, active);
        }
    }

    MTS_INLINE auto eval_impl(const Interaction3f &it, Mask active) const {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        using StorageType = ek::Array<Float, Channels>;
        constexpr bool uses_srgb_model = is_spectral_v<Spectrum> && !Raw && Channels == 3;
        using ResultType = std::conditional_t<uses_srgb_model, UnpolarizedSpectrum, StorageType>;

        auto p = m_to_local * it.p;
        if (ek::none_or<false>(active))
            return ek::zero<ResultType>();
        ResultType result = interpolate(p, it.wavelengths, active);
        return result & active;
    }

    template <typename T> T wrap(const T &value) const {
        ScalarVector3i res = resolution();
        if (m_wrap_mode == WrapMode::Clamp) {
            return clamp(value, 0, res - 1);
        } else {
            T div = T(m_inv_resolution_x(value.x()),
                      m_inv_resolution_y(value.y()),
                      m_inv_resolution_z(value.z())),
              mod = value - div * res;

            ek::masked(mod, mod < 0) += T(res);

            if (m_wrap_mode == WrapMode::Mirror)
                mod = ek::select(ek::eq(div & 1, 0) ^ (value < 0), mod, res - 1 - mod);

            return mod;
        }
    }

    /**
     * Taking a 3D point in [0, 1)^3, estimates the grid's value at that
     * point using trilinear interpolation.
     *
     * The passed `active` mask must disable lanes that are not within the
     * domain.
     */
    MTS_INLINE auto interpolate(Point3f p, const Wavelength &wavelengths,
                                Mask active) const {
        constexpr bool uses_srgb_model = is_spectral_v<Spectrum> && !Raw && Channels == 3;
        using StorageType = std::conditional_t<uses_srgb_model, ek::Array<Float, 4>, ek::Array<Float, Channels>>;
        using ResultType = std::conditional_t<uses_srgb_model, UnpolarizedSpectrum, StorageType>;

        if constexpr (!ek::is_array_v<Mask>)
            active = true;

        ScalarVector3i res = resolution();
        const uint32_t nx = res.x();
        const uint32_t ny = res.y();

        if (m_filter_type == FilterType::Trilinear) {
            using Int8  = ek::Array<Int32, 8>;
            using Int38 = ek::Array<Int8, 3>;

            // Scale to bitmap resolution and apply shift
            p = ek::fmadd(p, res, -.5f);

            // Integer pixel positions for trilinear interpolation
            Vector3i p_i  = ek::floor2int<Vector3i>(p);

            // Interpolation weights
            Point3f w1 = p - Point3f(p_i),
                    w0 = 1.f - w1;

            Int38 pi_i_w = wrap(Int38(Int8(0, 1, 0, 1, 0, 1, 0, 1) + p_i.x(),
                                      Int8(0, 0, 1, 1, 0, 0, 1, 1) + p_i.y(),
                                      Int8(0, 0, 0, 0, 1, 1, 1, 1) + p_i.z()));

            Int8 index = (pi_i_w.z() * ny + pi_i_w.y()) * nx + pi_i_w.x();

            // Load 8 grid positions to perform trilinear interpolation
            auto d000 = ek::gather<StorageType>(m_data.array(), index[0], active),
                 d100 = ek::gather<StorageType>(m_data.array(), index[1], active),
                 d010 = ek::gather<StorageType>(m_data.array(), index[2], active),
                 d110 = ek::gather<StorageType>(m_data.array(), index[3], active),
                 d001 = ek::gather<StorageType>(m_data.array(), index[4], active),
                 d101 = ek::gather<StorageType>(m_data.array(), index[5], active),
                 d011 = ek::gather<StorageType>(m_data.array(), index[6], active),
                 d111 = ek::gather<StorageType>(m_data.array(), index[7], active);

            ResultType v000, v001, v010, v011, v100, v101, v110, v111;
            Float scale = 1.f;
            if constexpr (uses_srgb_model) {
                v000 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d000), wavelengths);
                v100 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d100), wavelengths);
                v010 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d010), wavelengths);
                v110 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d110), wavelengths);
                v001 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d001), wavelengths);
                v101 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d101), wavelengths);
                v011 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d011), wavelengths);
                v111 = srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(d111), wavelengths);
                // Interpolate scaling factor
                Float f00 = ek::fmadd(w0.x(), d000.w(), w1.x() * d100.w()),
                      f01 = ek::fmadd(w0.x(), d001.w(), w1.x() * d101.w()),
                      f10 = ek::fmadd(w0.x(), d010.w(), w1.x() * d110.w()),
                      f11 = ek::fmadd(w0.x(), d011.w(), w1.x() * d111.w());
                Float f0  = ek::fmadd(w0.y(), f00, w1.y() * f10),
                      f1  = ek::fmadd(w0.y(), f01, w1.y() * f11);
                    scale = ek::fmadd(w0.z(), f0, w1.z() * f1);
            } else {
                v000 = d000; v001 = d001; v010 = d010; v011 = d011;
                v100 = d100; v101 = d101; v110 = d110; v111 = d111;
                ENOKI_MARK_USED(scale);
                ENOKI_MARK_USED(wavelengths);
            }

            // Trilinear interpolation
            ResultType v00 = ek::fmadd(w0.x(), v000, w1.x() * v100),
                       v01 = ek::fmadd(w0.x(), v001, w1.x() * v101),
                       v10 = ek::fmadd(w0.x(), v010, w1.x() * v110),
                       v11 = ek::fmadd(w0.x(), v011, w1.x() * v111);
            ResultType v0  = ek::fmadd(w0.y(), v00, w1.y() * v10),
                       v1  = ek::fmadd(w0.y(), v01, w1.y() * v11);
            ResultType result = ek::fmadd(w0.z(), v0, w1.z() * v1);

            if constexpr (uses_srgb_model)
                result *= scale;

            return result;
        } else {
            // Scale to volume resolution, no shift
            p *= res;

            // Integer voxel positions for lookup
            Vector3i p_i   = ek::floor2int<Vector3i>(p),
                     p_i_w = wrap(p_i);

            Int32 index = (p_i_w.z() * ny + p_i_w.y()) * nx + p_i_w.x();
            StorageType v = ek::gather<StorageType>(m_data.array(), index, active);

            if constexpr (uses_srgb_model)
                return v.w() * srgb_model_eval<UnpolarizedSpectrum>(ek::head<3>(v), wavelengths);
            else
                return v;

        }
    }

    ScalarFloat max() const override { return m_max; }

    bool is_spatially_varying() const override { return true; }

    ScalarVector3i resolution() const override {
        auto shape = m_data.shape();
        return { (int) shape[2], (int) shape[1], (int) shape[0] };
    };

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("data", m_data);
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        ScalarVector3i res = resolution();
        m_inv_resolution_x = ek::divisor<int32_t>(res.x());
        m_inv_resolution_y = ek::divisor<int32_t>(res.y());
        m_inv_resolution_z = ek::divisor<int32_t>(res.z());

        // Recompute maximum if necessary
        if (!m_fixed_max)
            m_max = (float) ek::hmax_nested(ek::detach(m_data.array()));
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "GridVolume[" << std::endl
            << "  to_local = " << m_to_local << "," << std::endl
            << "  bbox = " << string::indent(m_bbox) << "," << std::endl
            << "  dimensions = " << resolution() << "," << std::endl
            << "  max = " << m_max << "," << std::endl
            << "  channels = " << m_data.shape()[3] << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    TensorXf m_data;
    bool m_fixed_max = false;
    ek::divisor<int32_t> m_inv_resolution_x,
                         m_inv_resolution_y,
                         m_inv_resolution_z;
    ScalarFloat m_max;
    FilterType m_filter_type;
    WrapMode m_wrap_mode;
};

MTS_IMPLEMENT_CLASS_VARIANT(GridVolume, Volume)
MTS_EXPORT_PLUGIN(GridVolume, "GridVolume texture")

NAMESPACE_BEGIN(detail)
template <uint32_t Channels, bool Raw>
constexpr const char * gridvolume_class_name() {
    if constexpr (!Raw) {
        if constexpr (Channels == 1)
            return "GridVolumeImpl_1_0";
        else if constexpr (Channels == 3)
            return "GridVolumeImpl_3_0";
        else
            return "GridVolumeImpl_6_0";
    } else {
        if constexpr (Channels == 1)
            return "GridVolumeImpl_1_1";
        else if constexpr (Channels == 3)
            return "GridVolumeImpl_3_1";
        else
            return "GridVolumeImpl_6_1";
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
