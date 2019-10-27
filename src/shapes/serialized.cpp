#include <mitsuba/render/mesh.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/zstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/timer.h>

NAMESPACE_BEGIN(mitsuba)

#define MTS_FILEFORMAT_HEADER     0x041C
#define MTS_FILEFORMAT_VERSION_V3 0x0003
#define MTS_FILEFORMAT_VERSION_V4 0x0004

class SerializedMesh final : public Mesh {
public:
    enum ETriMeshFlags {
        EHasNormals      = 0x0001,
        EHasTexcoords    = 0x0002,
        EHasTangents     = 0x0004, // unused
        EHasColors       = 0x0008,
        EFaceNormals     = 0x0010,
        ESinglePrecision = 0x1000,
        EDoublePrecision = 0x2000
    };

    SerializedMesh(const Properties &props) : Mesh(props) {
        auto fail = [&](const std::string &descr) {
            Throw("Error while loading serialized file \"%s\": %s!", m_name, descr);
        };

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();

        Log(Debug, "Loading mesh from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        // Object-space to world-space transformation
        Transform4f to_world = props.transform("to_world", Transform4f());

        /// When the file contains multiple meshes, this index specifies which one to load
        int shape_index = props.int_("shape_index", 0);
        if (shape_index < 0)
            fail("shape index must be nonnegative!");

        m_name = tfm::format("%s@%i", file_path.filename(), shape_index);

        ref<Stream> stream = new FileStream(file_path);
        Timer timer;
        stream->set_byte_order(Stream::ELittleEndian);

        short format = 0, version = 0;
        stream->read(format);
        stream->read(version);

        if (format != MTS_FILEFORMAT_HEADER)
            fail("encountered an invalid file format!");

        if (version != MTS_FILEFORMAT_VERSION_V3 &&
            version != MTS_FILEFORMAT_VERSION_V4)
            fail("encountered an incompatible file version!");

        if (shape_index != 0) {
            size_t file_size = stream->size();

            /* Determine the position of the requested substream. This
               is stored at the end of the file */
            stream->seek(file_size - sizeof(uint32_t));

            uint32_t count = 0;
            stream->read(count);

            if (shape_index > (int) count)
                fail(tfm::format("Unable to unserialize mesh, shape index is "
                                 "out of range! (requested %i out of 0..%i)",
                                 shape_index, count - 1));

            // Seek to the correct position
            if (version == MTS_FILEFORMAT_VERSION_V4) {
                stream->seek(file_size -
                             sizeof(uint64_t) * (count - shape_index) -
                             sizeof(uint32_t));
                size_t offset = 0;
                stream->read(offset);
                stream->seek(offset);
            } else {
                Assert(version == MTS_FILEFORMAT_VERSION_V3);
                stream->seek(file_size -
                             sizeof(uint32_t) * (count - shape_index + 1));
                uint32_t offset = 0;
                stream->read(offset);
                stream->seek(offset);
            }
            stream->skip(sizeof(short) * 2); // Skip the header
        }

        stream = new ZStream(stream);
        stream->set_byte_order(Stream::ELittleEndian);

        uint32_t flags = 0;
        stream->read(flags);
        if (version == MTS_FILEFORMAT_VERSION_V4) {
            char ch = 0;
            m_name = "";
            do {
                stream->read(ch);
                if (ch == 0)
                    break;
                m_name += ch;
            } while (true);
        }

        size_t vertex_count, face_count;
        stream->read(vertex_count);
        stream->read(face_count);

        m_vertex_struct = new Struct();
        for (auto name : { "x", "y", "z" })
            m_vertex_struct->append(name, Struct::EFloat);

        if (!m_disable_vertex_normals) {
            for (auto name : { "nx", "ny", "nz" })
                m_vertex_struct->append(name, Struct::EFloat);
            m_normal_offset = (Index) m_vertex_struct->offset("nx");
        }

        if (flags & EHasTexcoords) {
            for (auto name : { "u", "v" })
                m_vertex_struct->append(name, Struct::EFloat);
            m_texcoord_offset = (Index) m_vertex_struct->offset("u");
        }

        if (flags & EHasColors) {
            for (auto name : { "r", "g", "b" })
                m_vertex_struct->append(name, Struct::EFloat);
            m_color_offset = (Index) m_vertex_struct->offset("r");
        }

        m_face_struct = new Struct();
        for (size_t i = 0; i < 3; ++i)
            m_face_struct->append(tfm::format("i%i", i),
                                  struct_traits<Index>::value);

        m_vertex_size = (Size) m_vertex_struct->size();
        m_vertex_count = (Size) vertex_count;
        m_vertices = VertexHolder(new uint8_t[(m_vertex_count + 1) * m_vertex_size]);

        m_face_size = (Size) m_face_struct->size();
        m_face_count = (Size) face_count;
        m_faces = FaceHolder(new uint8_t[(m_face_count + 1) * m_face_size]);

        bool double_precision = flags & EDoublePrecision;
        read_helper(stream, double_precision, m_vertex_struct->offset("x"), 3);

        if (flags & EHasNormals) {
            if (m_disable_vertex_normals)
                // Skip over vertex normals provided in the file.
                advance_helper(stream, double_precision, 3);
            else
                read_helper(stream, double_precision,
                            m_vertex_struct->offset("nx"), 3);
        }

        if (flags & EHasTexcoords)
            read_helper(stream, double_precision, m_vertex_struct->offset("u"), 2);

        if (flags & EHasColors)
            read_helper(stream, double_precision, m_vertex_struct->offset("r"), 3);

        stream->read(m_faces.get(), m_face_count * sizeof(Index) * 3);

        Log(Debug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_name, m_face_count, m_vertex_count,
            util::mem_string(m_face_count * m_face_struct->size() +
                             m_vertex_count * m_vertex_struct->size()),
            util::time_string(timer.value())
        );

        // Post-processing
        for (Size i = 0; i < m_vertex_count; ++i) {
            Point3f p = to_world * vertex_position(i);
            store_unaligned(vertex(i), p);
            m_bbox.expand(p);

            if (has_vertex_normals()) {
                Normal3f n = normalize(to_world * vertex_normal(i));
                store_unaligned(vertex(i) + m_normal_offset, n);
            }

            if (has_vertex_texcoords()) {
                Point2f uv = vertex_texcoord(i);
                store_unaligned(vertex(i) + m_texcoord_offset, uv);
            }
        }

        if (!m_disable_vertex_normals && (flags & EHasNormals) == 0)
            recompute_vertex_normals();

        if (is_emitter())
            emitter()->set_shape(this);
    }

    void read_helper(Stream *stream, bool dp, size_t offset, size_t dim) {
        if (dp) {
            std::unique_ptr<double[]> values(new double[m_vertex_count * dim]);
            stream->read_array(values.get(), m_vertex_count * dim);

            if constexpr (std::is_same_v<Float, double>) {
                for (size_t i = 0; i < m_vertex_count; ++i) {
                    const double *src = values.get() + dim * i;
                    double *dst = (double *) (vertex(i) + offset);
                    memcpy(dst, src, sizeof(double) * dim);
                }
            } else {
                for (size_t i = 0; i < m_vertex_count; ++i) {
                    const double *src = values.get() + dim * i;
                    float *dst = (float *) (vertex(i) + offset);
                    for (size_t d = 0; d < dim; ++d)
                        dst[d] = (float) src[d];
                }
            }
        } else {
            std::unique_ptr<float[]> values(new float[m_vertex_count * dim]);
            stream->read_array(values.get(), m_vertex_count * dim);

            if constexpr (std::is_same_v<Float, float>) {
                for (size_t i = 0; i < m_vertex_count; ++i) {
                    const float *src = values.get() + dim * i;
                    void *dst = vertex(i) + offset;
                    memcpy(dst, src, sizeof(float) * dim);
                }
            } else {
                for (size_t i = 0; i < m_vertex_count; ++i) {
                    const float *src = values.get() + dim * i;
                    double *dst = (double *) (vertex(i) + offset);
                    for (size_t d = 0; d < dim; ++d)
                        dst[d] = (double) src[d];
                }
            }
        }
    }

    /**
     * Simply advances the stream without outputing to the mesh.
     * Since compressed streams do not provide `tell` and `seek`
     * implementations, we have to read and discard the data.
     */
    void advance_helper(Stream *stream, bool dp, size_t dim) {
        if (dp) {
            std::unique_ptr<double[]> values(new double[m_vertex_count * dim]);
            stream->read_array(values.get(), m_vertex_count * dim);
        } else {
            std::unique_ptr<float[]> values(new float[m_vertex_count * dim]);
            stream->read_array(values.get(), m_vertex_count * dim);
        }
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS(SerializedMesh, Mesh)
MTS_EXPORT_PLUGIN(SerializedMesh, "Serialized mesh file")

NAMESPACE_END(mitsuba)
