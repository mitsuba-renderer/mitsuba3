#include <mitsuba/render/mesh.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/zstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/timer.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-serialized:

Serialized mesh loader (:monosp:`serialized`)
---------------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the OBJ file that should be loaded
 * - shape_index
   - |int|
   - A :monosp:`.serialized` file may contain several separate meshes. This parameter
     specifies which one should be loaded. (Default: 0, i.e. the first one)
 * - face_normals
   - |bool|
   - When set to |true|, any existing or computed vertex normals are
     discarded and \emph{face normals} will instead be used during rendering.
     This gives the rendered object a faceted appearance.(Default: |false|)
 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none, i.e. object space = world space)

The serialized mesh format represents the most space and time-efficient way
of getting geometry information into Mitsuba 2. It stores indexed triangle meshes
in a lossless gzip-based encoding that (after decompression) nicely matches up
with the internally used data structures. Loading such files is considerably
faster than the :ref:`ply <shape-ply>` plugin and orders of magnitude faster than
the :ref:`obj <shape-obj>` plugin.

Format description
******************

The :monosp:`serialized` file format uses the little endian encoding, hence
all fields below should be interpreted accordingly. The contents are structured as
follows:

.. figtable::
    :label: table-serialized-format

    .. list-table::
        :widths: 20 80
        :header-rows: 1

        * - Type
          - Content
        * - :monosp:`uint16`
          - File format identifier: :code:`0x041C`
        * - :monosp:`uint16`
          - File version identifier. Currently set to :code:`0x0004`
        * - :math:`\rightarrow`
          - From this point on, the stream is compressed by the :monosp:`DEFLATE` algorithm.
        * - :math:`\rightarrow`
          - The used encoding is that of the :monosp:`zlib` library.
        * - :monosp:`uint32`
          - An 32-bit integer whose bits can be used to specify the following flags:

            - :code:`0x0001`: The mesh data includes per-vertex normals
            - :code:`0x0002`: The mesh data includes texture coordinates
            - :code:`0x0008`: The mesh data includes vertex colors
            - :code:`0x0010`: Use face normals instead of smothly interpolated vertex normals.
              Equivalent to specifying :monosp:`face_normals=true` to the plugin.
            - :code:`0x1000`: The subsequent content is represented in single precision
            - :code:`0x2000`: The subsequent content is represented in double precision
        * - :monosp:`string`
          - A null-terminated string (utf-8), which denotes the name of the shape.
        * - :monosp:`uint64`
          - Number of vertices in the mesh
        * - :monosp:`uint64`
          - Number of triangles in the mesh
        * - :monosp:`array`
          - Array of all vertex positions (X, Y, Z, X, Y, Z, ...) specified in binary single or
            double precision format (as denoted by the flags)
        * - :monosp:`array`
          - Array of all vertex normal directions (X, Y, Z, X, Y, Z, ...) specified in binary single
            or double precision format. When the mesh has no vertex normals, this field is omitted.
        * - :monosp:`array`
          - Array of all vertex texture coordinates (U, V, U, V, ...) specified in binary single or
            double precision format. When the mesh has no texture coordinates, this field is omitted.
        * - :monosp:`array`
          - Array of all vertex colors (R, G, B, R, G, B, ...) specified in binary single or double
            precision format. When the mesh has no vertex colors, this field is omitted.
        * - :monosp:`array`
          - Indexed triangle data (:code:`[i1, i2, i3]`, :code:`[i1, i2, i3]`, ..) specified in
            :monosp:`uint32` or in :monosp:`uint64` format (the latter is used when the number of
            vertices exceeds :code:`0xFFFFFFFF`).

Multiple shapes
***************

It is possible to store multiple meshes in a single :monosp:`.serialized`
file. This is done by simply concatenating their data streams,
where every one is structured according to the above description.
Hence, after each mesh, the stream briefly reverts back to an
uncompressed format, followed by an uncompressed header, and so on.
This is neccessary for efficient read access to arbitrary sub-meshes.

End-of-file dictionary
**********************
In addition to the previous table, a :monosp:`.serialized` file also concludes with a brief summary
at the end of the file, which specifies the starting position of each sub-mesh:

.. figtable::
    :label: table-serialized-end-of-file

    .. list-table::
        :widths: 20 80
        :header-rows: 1

        * - Type
          - Content
        * - :monosp:`uint64`
          - File offset of the first mesh (in bytes)---this is always zero.
        * - :monosp:`uint64`
          - File offset of the second mesh
        * - :math:`\cdots`
          - :math:`\cdots`
        * - :monosp:`uint64`
          - File offset of the last sub-shape
        * - :monosp:`uint32`
          - Total number of meshes in the :monosp:`.serialized` file

 */

#define MTS_FILEFORMAT_HEADER     0x041C
#define MTS_FILEFORMAT_VERSION_V3 0x0003
#define MTS_FILEFORMAT_VERSION_V4 0x0004

template <typename Float, typename Spectrum>
class SerializedMesh final : public Mesh<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Mesh,m_name, m_bbox, m_to_world, m_vertex_count, m_face_count,
                    m_vertex_positions_buf, m_vertex_normals_buf, m_vertex_texcoords_buf,
                    m_faces_buf, m_disable_vertex_normals, has_vertex_normals, has_vertex_texcoords,
                    recompute_vertex_normals, vertex_position, vertex_normal, set_children)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using typename Base::InputFloat;
    using typename Base::FloatStorage;
    using typename Base::InputPoint3f;
    using typename Base::InputNormal3f;

    enum class TriMeshFlags {
        HasNormals      = 0x0001,
        HasTexcoords    = 0x0002,
        HasTangents     = 0x0004, // unused
        HasColors       = 0x0008,
        FaceNormals     = 0x0010,
        SinglePrecision = 0x1000,
        DoublePrecision = 0x2000
    };

    constexpr bool has_flag(TriMeshFlags flags, TriMeshFlags f) {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(f)) != 0;
    }
    constexpr bool has_flag(uint32_t flags, TriMeshFlags f) {
        return (flags & static_cast<uint32_t>(f)) != 0;
    }

    SerializedMesh(const Properties &props) : Base(props) {
        auto fail = [&](const std::string &descr) {
            Throw("Error while loading serialized file \"%s\": %s!", m_name, descr);
        };

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();

        Log(Debug, "Loading mesh from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

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

        m_vertex_count = (ScalarSize) vertex_count;
        m_face_count = (ScalarSize) face_count;

        m_vertex_positions_buf = empty<FloatStorage>(m_vertex_count * 3);
        if (!m_disable_vertex_normals)
            m_vertex_normals_buf = empty<FloatStorage>(m_vertex_count * 3);
        if (has_flag(flags, TriMeshFlags::HasTexcoords))
            m_vertex_texcoords_buf = empty<FloatStorage>(m_vertex_count * 2);

        m_faces_buf = empty<DynamicBuffer<UInt32>>(m_face_count * 3);

        m_vertex_positions_buf.managed();
        m_vertex_normals_buf.managed();
        m_vertex_texcoords_buf.managed();
        m_faces_buf.managed();

        if constexpr (is_cuda_array_v<Float>)
            cuda_sync();

        bool double_precision = has_flag(flags, TriMeshFlags::DoublePrecision);

        read_helper(stream, double_precision, m_vertex_positions_buf.data(), 3);

        if (has_flag(flags, TriMeshFlags::HasNormals)) {
            if (m_disable_vertex_normals)
                // Skip over vertex normals provided in the file.
                advance_helper(stream, double_precision, 3);
            else
                read_helper(stream, double_precision, m_vertex_normals_buf.data(), 3);
        }

        if (has_flag(flags, TriMeshFlags::HasTexcoords))
            read_helper(stream, double_precision, m_vertex_texcoords_buf.data(), 2);

        if (has_flag(flags, TriMeshFlags::HasColors))
            advance_helper(stream, double_precision, 3); // TODO

        stream->read(m_faces_buf.data(), m_face_count * sizeof(ScalarIndex) * 3);

        size_t vertex_data_bytes = 3 * sizeof(InputFloat);
        if (has_vertex_normals())
            vertex_data_bytes += 3 * sizeof(InputFloat);
        if (has_vertex_texcoords())
            vertex_data_bytes += 2 * sizeof(InputFloat);

        Log(Debug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_name, m_face_count, m_vertex_count,
            util::mem_string(m_face_count * 3 * sizeof(ScalarIndex) +
                             m_vertex_count * vertex_data_bytes),
            util::time_string(timer.value())
        );

        // Post-processing
        InputFloat* position_ptr = m_vertex_positions_buf.data();
        InputFloat* normal_ptr   = m_vertex_normals_buf.data();
        for (ScalarSize i = 0; i < m_vertex_count; ++i) {
            InputPoint3f p = m_to_world.transform_affine(vertex_position(i));
            store_unaligned(position_ptr, p);
            position_ptr += 3;
            m_bbox.expand(p);

            if (has_vertex_normals()) {
                InputNormal3f n = normalize(m_to_world.transform_affine(vertex_normal(i)));
                store_unaligned(normal_ptr, n);
                normal_ptr += 3;
            }
        }

        if (!m_disable_vertex_normals && !has_flag(flags, TriMeshFlags::HasNormals)) {
            Timer timer2;
            recompute_vertex_normals();
            Log(Debug, "\"%s\": computed vertex normals (took %s)", m_name,
                util::time_string(timer2.value()));
        }

        set_children();
    }

    void read_helper(Stream *stream, bool dp, InputFloat* dst, size_t dim) {
        if (dp) {
            std::unique_ptr<double[]> values(new double[m_vertex_count * dim]);
            stream->read_array(values.get(), m_vertex_count * dim);
            for (size_t i = 0; i < m_vertex_count * dim; ++i)
                dst[i] = (float) values[i];
        } else {
            std::unique_ptr<float[]> values(new float[m_vertex_count * dim]);
            stream->read_array(values.get(), m_vertex_count * dim);
            memcpy(dst, values.get(), m_vertex_count * sizeof(InputFloat) * dim);
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

MTS_IMPLEMENT_CLASS_VARIANT(SerializedMesh, Mesh)
MTS_EXPORT_PLUGIN(SerializedMesh, "Serialized mesh file")
NAMESPACE_END(mitsuba)
