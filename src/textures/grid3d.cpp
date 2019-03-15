#include <fstream>
#include <sstream>

#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/thread.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/texture3d.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
#if defined(SINGLE_PRECISION)
inline Float stof(const std::string &s) { return std::stof(s); }
#else
inline Float stof(const std::string &s) { return std::stod(s); }
#endif
NAMESPACE_END(detail)

/**
 * Interpolated 3D grid texture.
 * Values can be read from the Mitsuba 1 binary volume format, or a simple
 * comma-separated string (for testing).
 *
 * The data must be ordered so that the following C-style (row-major) indexing
 * operation makes sense after the file has been mapped into memory:
 *     data[((zpos*yres + ypos)*xres + xpos)*channels + chan]}
 *     where (xpos, ypos, zpos, chan) denotes the lookup location.
 */
class Grid3D final : public Texture3D {
public:
    explicit Grid3D(const Properties &props)
        : Texture3D(props), m_nx(0), m_ny(0), m_nz(0) {

        if (props.has_property("filename"))
            read_binary_file(props);
        else
            parse_string_grid(props);

#if defined(MTS_ENABLE_AUTODIFF)
        // Copy parsed data over to the GPU
        m_data_d = CUDAArray<Float>::copy(m_data.data(), m_size);
#endif
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "data", m_data_d);
    }

    /// Note: this assumes that the size has not changed.
    void parameters_changed() override {
        m_mean = (double) hsum(m_data_d)[0] / (double) m_size;
        m_max  = hmax(m_data_d)[0];
    }
#endif

    /**
     * Taking a 3D point in [0, 1)^3, estimates the grid's value at that
     * point using trilinear interpolation.
     *
     * The passed `active` mask must disable lanes that are not within the
     * domain.
     */
    template <typename Point3, typename Value = value_t<Point3>>
    MTS_INLINE auto trilinear_interpolation(Point3 p, const mask_t<Value> &active) const {
        using Index    = replace_scalar_t<Value, uint32_t>;
        using Index3   = replace_scalar_t<Point3, uint32_t>;
        using Spectrum = mitsuba::Spectrum<Value>;
        using Mask     = mask_t<Value>;

        Index3 resolution(m_nx, m_ny, m_nz);
        p = enoki::max(0.f, p * resolution - 0.5f);
        // Integer part
        Index3 pi  = enoki::floor2int<Index3>(p);
        Index3 pi2 = enoki::min(pi + 1, resolution - 1);
        // Fractional part
        Point3 f  = p - Point3(pi);
        Point3 rf = 1.f - f;

        auto *d      = (Float *) data<Value>().data();
        auto wgather = [&](const Index &index, const Mask &active) {
            if constexpr (!is_diff_array_v<Index>) {
                return gather<Value>(d, index, active);
            }
#if defined(MTS_ENABLE_AUTODIFF)
            else {
                return gather<Value>(m_data_d, index, active);
            }
#endif
        };

        Value d000 = wgather((pi.z() * m_ny + pi.y()) * m_nx + pi.x(), active),
              d001 = wgather((pi.z() * m_ny + pi.y()) * m_nx + pi2.x(), active),
              d010 = wgather((pi.z() * m_ny + pi2.y()) * m_nx + pi.x(), active),
              d011 = wgather((pi.z() * m_ny + pi2.y()) * m_nx + pi2.x(), active),
              d100 = wgather((pi2.z() * m_ny + pi.y()) * m_nx + pi.x(), active),
              d101 = wgather((pi2.z() * m_ny + pi.y()) * m_nx + pi2.x(), active),
              d110 = wgather((pi2.z() * m_ny + pi2.y()) * m_nx + pi.x(), active),
              d111 = wgather((pi2.z() * m_ny + pi2.y()) * m_nx + pi2.x(), active);

        Value interpolated = ((d000 * rf.x() + d001 * f.x()) * rf.y() +
                              (d010 * rf.x() + d011 * f.x()) * f.y()) *
                                 rf.z() +
                             ((d100 * rf.x() + d101 * f.x()) * rf.y() +
                              (d110 * rf.x() + d111 * f.x()) * f.y()) *
                                 f.z();
        return Spectrum(interpolated);
    }

    template <typename Interaction,
              typename Value    = typename Interaction::Value,
              typename Spectrum = mitsuba::Spectrum<Value>>
    MTS_INLINE Spectrum eval_impl(const Interaction &it,
                                  mask_t<Value> active) const {
        auto p          = m_world_to_local * it.p;
        Spectrum result = 0.f;
        active &= all((p >= 0) && (p <= 1));
        if (none_or<false>(active))
            return result;
        result[active] = trilinear_interpolation(p, active);
        return result;
    }

    Float mean() const override { return (Float) m_mean; }
    Float max() const override { return m_max; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Grid3D[" << std::endl
            << "  world_to_local = " << m_world_to_local << "," << std::endl
            << "  dimensions = " << m_nx << "x" << m_ny << "x" << m_nz << ","
            << std::endl
            << "  mean = " << m_mean << "," << std::endl
            << "  max = " << m_max << "," << std::endl
            << "  data = " << m_data << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_TEXTURE_3D()
    MTS_AUTODIFF_GETTER(data, m_data, m_data_d)
    MTS_DECLARE_CLASS()

protected:
    FloatX m_data;
    size_t m_nx, m_ny, m_nz, m_size;
    double m_mean;
    Float m_max;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_data_d;
#endif

private:
    template <typename T> T read(std::ifstream &f) {
        T v;
        f.read(reinterpret_cast<char *>(&v), sizeof(v));
        return v;
    }

    Transform4f bbox_transform(const BoundingBox3f bbox) {
        Vector3f d        = rcp(bbox.max - bbox.min);
        auto scale_transf = Transform4f::scale(d);
        Vector3f t = -1.f * Vector3f(bbox.min.x(), bbox.min.y(), bbox.min.z());
        auto translation = Transform4f::translate(t);
        return scale_transf * translation;
    }

    void read_binary_file(const Properties &props) {
        std::string filename = props.string("filename");
        if (props.has_property("side") || props.has_property("nx") ||
            props.has_property("ny") || props.has_property("nz"))
            Throw("Grid dimensions are already specified in the binary volume "
                  "file, cannot override them with properties side, nx, ny or "
                  "nz.");
        auto fs            = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        std::ifstream f(file_path.string(), std::ios::binary);
        char header[3];
        f.read(header, sizeof(char) * 3);
        if (header[0] != 'V' || header[1] != 'O' || header[2] != 'L')
            Throw("Invalid volume file %s", filename);
        auto version = read<uint8_t>(f);
        if (version != 3)
            Throw("Invalid version, currently only version 3 is supported");

        auto type = read<int32_t>(f);
        if (type != 1)
            Throw("Wrong type, currently only type == 1 (Float32) data is "
                  "supported");

        m_nx   = read<int32_t>(f);
        m_ny   = read<int32_t>(f);
        m_nz   = read<int32_t>(f);
        m_size = m_nx * m_ny * m_nz;
        if (m_size < 1)
            Throw("Invalid grid dimensions: %d x %d x %d < 1 (must have at "
                  "least one value)",
                  m_nx, m_ny, m_nz);

        auto channels = read<int32_t>(f);
        if (channels != 1)
            Throw("Currently only single-channel volumes are supported");
        float dims[6];
        f.read(reinterpret_cast<char *>(dims), sizeof(float) * 6);
        // If requested, use the transform specified in the grid file
        if (props.bool_("use_grid_bbox", false)) {
            BoundingBox3f bbox(Point3f(dims[0], dims[1], dims[2]),
                               Point3f(dims[3], dims[4], dims[5]));
            m_world_to_local = bbox_transform(bbox) * m_world_to_local;
            update_bbox();
        }

        set_slices(m_data, m_size);
        m_mean = 0.;
        m_max  = 0.f;
        for (size_t i = 0; i < m_size; ++i) {
            auto val         = read<float>(f);
            slice(m_data, i) = val;
            m_mean += (double) val;
            m_max = std::max(m_max, (Float) val);
        }
        m_mean /= (double) m_size;
    }

    void parse_string_grid(const Properties &props) {
        if (props.has_property("side"))
            m_nx = m_ny = m_nz = props.size_("side");
        m_nx   = props.size_("nx", m_nx);
        m_ny   = props.size_("ny", m_ny);
        m_nz   = props.size_("nz", m_nz);
        m_size = m_nx * m_ny * m_nz;
        if (m_size < 1)
            Throw("Invalid grid dimensions: %d x %d x %d < 1 (must have at "
                  "least one value)",
                  m_nx, m_ny, m_nz);

        set_slices(m_data, m_size);
        std::vector<std::string> tokens = string::tokenize(props.string("values"), ",");
        if (tokens.size() != m_size)
            Throw("Invalid token count: expected %d, found %d in "
                  "comma-separated list:\n  \"%s\"",
                  m_size, tokens.size(), props.string("values"));

        m_mean = 0.;
        m_max  = 0.f;
        for (size_t i = 0; i < m_size; ++i) {
            Float val        = detail::stof(tokens[i]);
            slice(m_data, i) = val;
            m_mean += (double) val;
            m_max = std::max(m_max, (Float) val);
        }
        m_mean /= (double) m_size;
    }
};

MTS_IMPLEMENT_CLASS(Grid3D, Texture3D)
MTS_EXPORT_PLUGIN(Grid3D, "Grid 3D texture with interpolation")

NAMESPACE_END(mitsuba)
