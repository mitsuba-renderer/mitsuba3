#include <mitsuba/render/mesh.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/zstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/profiler.h>

NAMESPACE_BEGIN(mitsuba)

/*
 * Serialized mesh loader (legacy). Reads the ``.serialized`` format of
 * earlier Mitsuba versions, which stores single-indexed triangle meshes in
 * a gzip-based encoding. New files should use the ``packed`` format.
 *
 * Parameters: filename, shape_index (default 0), face_normals,
 * flip_normals, to_world.
 */

#define MI_FILEFORMAT_HEADER     0x041C
#define MI_FILEFORMAT_VERSION_V3 0x0003
#define MI_FILEFORMAT_VERSION_V4 0x0004

/// Flag word of the encoding
enum class TriMeshFlags : uint32_t {
    HasNormals      = 0x0001,
    HasTexcoords    = 0x0002,
    HasTangents     = 0x0004, // unused
    HasColors       = 0x0008,
    FaceNormals     = 0x0010,
    SinglePrecision = 0x1000,
    DoublePrecision = 0x2000
};

MI_DECLARE_ENUM_OPERATORS(TriMeshFlags)

/// Read a null-terminated UTF-8 string
static std::string read_cstring(Stream *stream) {
    std::string result;
    char ch = 0;
    while (true) {
        stream->read(ch);
        if (ch == 0)
            return result;
        result += ch;
    }
}

template <typename Float, typename Spectrum>
class SerializedMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_filename, m_source_path, m_to_world,
                   m_vertex_count, m_face_count, m_face_normals,
                   m_flip_normals, has_face_normals, from_packed)
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f;
    using typename Base::InputNormal3f;
    using typename Base::InputVector2f;
    using typename Base::InputVector3f;

    SerializedMesh(const Properties &props) : Base(props) {
        auto fail = [&](const std::string &descr) {
            Throw("Error while loading serialized file \"%s\": %s!", m_filename, descr);
        };

        Log(Debug, "Loading mesh from \"%s\" ..", m_filename);

        /// When the file contains multiple meshes, this index specifies which one to load
        int shape_index = props.get<int>("shape_index", 0);
        if (shape_index < 0)
            fail("shape index must be nonnegative!");

        m_filename = tfm::format("%s@%i", m_source_path.filename(), shape_index);

        ref<Stream> stream = new FileStream(m_source_path);
        ScopedPhase phase(ProfilerPhase::LoadGeometry);
        Timer timer;
        stream->set_byte_order(Stream::ELittleEndian);

        short format = 0, version = 0;
        stream->read(format);
        stream->read(version);

        if (format != MI_FILEFORMAT_HEADER)
            fail("encountered an invalid file format!");

        if (version != MI_FILEFORMAT_VERSION_V3 &&
            version != MI_FILEFORMAT_VERSION_V4)
            fail("encountered an incompatible file version!");

        if (shape_index != 0) {
            size_t file_size = stream->size();

            // Determine the position of the requested substream. This
            // is stored at the end of the file
            stream->seek(file_size - sizeof(uint32_t));

            uint32_t count = 0;
            stream->read(count);

            if (shape_index >= (int) count)
                fail(tfm::format("Unable to unserialize mesh, shape index is "
                                 "out of range! (requested %i out of 0..%i)",
                                 shape_index, count - 1));

            // Seek to the correct position
            if (version >= MI_FILEFORMAT_VERSION_V4) {
                stream->seek(file_size -
                             sizeof(uint64_t) * (count - shape_index) -
                             sizeof(uint32_t));
                size_t offset = 0;
                stream->read(offset);
                stream->seek(offset);
            } else {
                Assert(version == MI_FILEFORMAT_VERSION_V3);
                stream->seek(file_size -
                             sizeof(uint32_t) * (count - shape_index + 1));
                uint32_t offset = 0;
                stream->read(offset);
                stream->seek(offset);
            }
            stream->skip(sizeof(short) * 2); // Skip the header
        }

        load(stream, version);

        Log(Debug, "\"%s\": read %i faces, %i vertices (in %s)",
            m_filename, m_face_count, m_vertex_count,
            util::time_string((float) timer.value()));
    }

    /// Load a version 3 or 4 mesh, which stores one tight array per
    /// quantity. Every vertex forms its own surface point.
    void load(Stream *stream_, short version) {
        ref<Stream> stream = new ZStream(stream_);
        stream->set_byte_order(Stream::ELittleEndian);

        uint32_t flags = 0;
        stream->read(flags);
        if (version == MI_FILEFORMAT_VERSION_V4) {
            std::string name = read_cstring(stream);
            if (!name.empty())
                m_filename = std::move(name);
        }

        size_t vertex_count, face_count;
        stream->read(vertex_count);
        stream->read(face_count);

        m_vertex_count = (ScalarSize) vertex_count;
        m_face_count   = (ScalarSize) face_count;

        bool double_precision = has_flag(flags, TriMeshFlags::DoublePrecision);
        bool has_normals      = has_flag(flags, TriMeshFlags::HasNormals);
        bool has_texcoords    = has_flag(flags, TriMeshFlags::HasTexcoords);
        bool has_colors       = has_flag(flags, TriMeshFlags::HasColors);

        bool store_normals = has_normals && !has_face_normals();

        PackedMesh pm(dr::backend_v<Float>, vertex_count, face_count,
                      make_layout(store_normals, has_texcoords));
        pm.set_transform(m_to_world.scalar(), m_flip_normals);
        m_flip_normals = false;
        m_to_world = ScalarAffineTransform4f();

        // The temporaries below are interleaved into the packed records
        // right after; a null destination skips a field
        std::vector<InputFloat> positions(vertex_count * 3);
        read_helper(stream, double_precision, positions.data(), 3);

        std::vector<InputFloat> normals;
        if (store_normals)
            normals.resize(vertex_count * 3);
        if (has_normals)
            read_helper(stream, double_precision,
                        store_normals ? normals.data() : nullptr, 3);

        std::vector<InputFloat> texcoords;
        if (has_texcoords) {
            texcoords.resize(vertex_count * 2);
            read_helper(stream, double_precision, texcoords.data(), 2);
        }

        if (has_colors)
            read_helper(stream, double_precision, nullptr, 3); // TODO

        std::vector<uint32_t> faces(face_count * 3);
        stream->read(faces.data(), face_count * sizeof(ScalarIndex) * 3);

        for (size_t i = 0; i < vertex_count; ++i) {
            InputPoint3f p = dr::load<InputPoint3f>(positions.data() + i * 3);

            InputNormal3f n(0.f, 0.f, 0.f);
            if (store_normals)
                n = dr::load<InputNormal3f>(normals.data() + i * 3);

            InputVector2f uv(0.f, 0.f);
            if (has_texcoords)
                uv = dr::load<InputVector2f>(texcoords.data() + i * 2);

            pm.set_vertex(i, p, n, uv);
        }

        for (size_t i = 0; i < face_count; ++i)
            pm.set_face(i, { faces[3 * i], faces[3 * i + 1],
                             faces[3 * i + 2] });

        stream_->close();
        from_packed(std::move(pm));
    }

    /// Read ``m_vertex_count * dim`` values. A null ``dst`` discards them:
    /// compressed streams have no ``seek``, so skipping means reading.
    void read_helper(Stream *stream, bool dp, InputFloat *dst, size_t dim) {
        size_t count = m_vertex_count * dim;
        if (!dp && dst) {
            stream->read_array(dst, count);
        } else if (dp) {
            std::unique_ptr<double[]> values(new double[count]);
            stream->read_array(values.get(), count);
            if (dst)
                for (size_t i = 0; i < count; ++i)
                    dst[i] = (float) values[i];
        } else {
            std::unique_ptr<float[]> values(new float[count]);
            stream->read_array(values.get(), count);
        }
    }

    MI_DECLARE_CLASS(SerializedMesh)
};

MI_EXPORT_PLUGIN(SerializedMesh)
NAMESPACE_END(mitsuba)
