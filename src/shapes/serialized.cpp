#include <mitsuba/render/mesh.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/zstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/profiler.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-serialized:

Serialized mesh loader (:monosp:`serialized`)
---------------------------------------------

.. pluginparameters::

 * - filename
   - |string|
   - Filename of the serialized file that should be loaded

 * - shape_index
   - |int|
   - A :monosp:`.serialized` file may contain several separate meshes. This parameter
     specifies which one should be loaded. (Default: 0, i.e. the first one)

 * - face_normals
   - |bool|
   - When set to |true|, any existing or computed vertex normals are
     discarded and \emph{face normals} will instead be used during rendering.
     This gives the rendered object a faceted appearance. (Default: |false|)

 * - flip_normals
   - |bool|
   - Is the mesh inverted, i.e. should the normal vectors be flipped? (Default:|false|, i.e.
     the normals point outside)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none, i.e. object space = world space)

In addition, this plugin exposes the standard mesh state parameters
documented in :ref:`sec-shape-mesh-parameters`.

The serialized mesh format represents the most space and time-efficient way
of getting geometry information into Mitsuba 3. It stores indexed triangle meshes
in a lossless gzip-based encoding that (after decompression) nicely matches up
with the internally used data structures. Loading such files is considerably
faster than the :ref:`ply <shape-ply>` plugin and orders of magnitude faster than
the :ref:`obj <shape-obj>` plugin.

Format description
******************

The :monosp:`serialized` file format uses the little endian encoding, hence
all fields below should be interpreted accordingly. The contents of a
version 5 file are structured as follows:

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
          - File version identifier. Currently set to :code:`0x0005`
        * - :math:`\rightarrow`
          - From this point on, the stream is compressed by the :monosp:`DEFLATE` algorithm.
        * - :math:`\rightarrow`
          - The used encoding is that of the :monosp:`zlib` library.
        * - :monosp:`uint32`
          - An 32-bit integer whose bits can be used to specify the following flags:

            - :code:`0x0007`: The low bits store the vertex record layout: :code:`0x1`
              denotes stored shading normals, :code:`0x2` shading tangents, and
              :code:`0x4` texture coordinates. Tangents occupy no lanes of their own:
              the frame lanes then hold the whole encoded shading frame instead of
              just the normal
            - :code:`0x0010`: Use face normals instead of smoothly interpolated vertex normals.
              Equivalent to specifying :monosp:`face_normals=true` to the plugin.
            - :code:`0x0020`: The stored shading normals were supplied by the user, rather
              than generated from the positions (in which case a position edit through the
              parameter interface recomputes them)
            - :code:`0x1000`: The subsequent content is represented in single precision
              (always set; version 5 files are single precision)
        * - :monosp:`string`
          - The name of the shape: a :monosp:`uint32` length followed by that many
            utf-8 bytes.
        * - :monosp:`uint64`
          - Number of vertices ``V`` in the mesh
        * - :monosp:`uint64`
          - Number of triangles ``F`` in the mesh
        * - :monosp:`uint64`
          - Number of distinct surface points ``P``, or 0 when the vertex-to-surface-point
            map is the identity and not stored
        * - :monosp:`uint64`
          - Number of normal groups ``N``, or 0 when the vertex-to-normal-group map is
            the identity and not stored
        * - :monosp:`array`
          - ``8 V`` single precision floats: the packed vertex records (position in lanes
            0-2, the shading normal or, with stored tangents, the encoded shading frame
            in lanes 3-5, texture coordinates in lanes 6-7; unused lanes are zero)
        * - :monosp:`array`
          - ``4 F`` :monosp:`uint32` face records: three vertex indices and the per-face
            BSDF index
        * - :monosp:`array`
          - ``V`` :monosp:`uint32` vertex-to-surface-point indices in ``[0, P)``. Omitted
            when ``P`` is 0.
        * - :monosp:`array`
          - ``V`` :monosp:`uint32` vertex-to-normal-group indices in ``[0, N)``. Omitted
            when ``N`` is 0.
        * - :monosp:`uint32`
          - Number of custom mesh attributes
        * - :monosp:`attribute`
          - Per attribute: a length-prefixed name whose ``vertex_`` or ``face_`` prefix
            selects the domain, a :monosp:`uint8` flag byte (bit 0: the values are
            sRGB-to-spectrum upsampling coefficients written by a spectral variant rather
            than raw values), a :monosp:`uint32` channel count ``dim`` in [1, 4], and
            ``V dim`` (or ``F dim``) single precision floats of attribute data

Version 3 and 4 files instead store a single-indexed triangle mesh: the flag
word (with :code:`0x0001` denoting normals, :code:`0x0002` texture
coordinates, :code:`0x0008` vertex colors and :code:`0x2000` double
precision data), the null-terminated shape name (version 4 only), the vertex and triangle
counts ``V`` and ``F``, and arrays of per-vertex positions, normals,
texture coordinates and colors, followed by ``3 F`` :monosp:`uint32` face
indices.

Multiple shapes
***************

It is possible to store multiple meshes in a single :monosp:`.serialized`
file. This is done by simply concatenating their data streams,
where every one is structured according to the above description.
Hence, after each mesh, the stream briefly reverts back to an
uncompressed format, followed by an uncompressed header, and so on.
This is necessary for efficient read access to arbitrary sub-meshes.

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

.. tabs::
    .. code-tab:: xml
        :name: serialized

        <shape type="serialized">
            <string name="filename" value="shape.serialized"/>
            <bsdf type='diffuse'/>
        </shape>

    .. code-tab:: python

        'type': 'serialized',
        'filename': 'shape.serialized',
        'material': {
            'type': 'diffuse',
        }
 */

/// Legacy format versions; the current one is `SerializedVersion`
#define MI_FILEFORMAT_VERSION_V3 0x0003
#define MI_FILEFORMAT_VERSION_V4 0x0004

/// Flag word of the legacy (version 3 and 4) encoding
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

/// Read a null-terminated UTF-8 string, as used by the legacy encoding
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

        if (format != SerializedMagic)
            fail("encountered an invalid file format!");

        if (version != MI_FILEFORMAT_VERSION_V3 &&
            version != MI_FILEFORMAT_VERSION_V4 &&
            version != SerializedVersion)
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

        if (version == SerializedVersion)
            load_v5(stream, props);
        else
            load_legacy(stream, version);

        Log(Debug, "\"%s\": read %i faces, %i vertices (in %s)",
            m_filename, m_face_count, m_vertex_count,
            util::time_string((float) timer.value()));
    }

    /// Load a version 3 or 4 mesh, which stores one tight array per
    /// quantity. Every vertex forms its own surface point.
    void load_legacy(Stream *stream_, short version) {
        ref<Stream> stream = new ZStream(stream_);
        stream->set_byte_order(Stream::ELittleEndian);

        uint32_t flags = 0;
        stream->read(flags);
        if (version == MI_FILEFORMAT_VERSION_V4)
            m_filename = read_cstring(stream);

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

    /**
     * Load a version 5 mesh, which stores the packed representation
     * verbatim: vertex and face records, the vertex -> surface point /
     * normal group maps, and custom attributes stream directly into the
     * staging storage. `Mesh::write_serialized()` is the writer.
     */
    void load_v5(Stream *stream_, const Properties &props) {
        ref<Stream> stream = new ZStream(stream_);
        stream->set_byte_order(Stream::ELittleEndian);

        uint32_t flags = 0;
        stream->read(flags);
        stream->read(m_filename);

        if (!(flags & (uint32_t) SerializedFlags::SinglePrecision))
            Throw("\"%s\": version 5 serialized meshes are stored in single "
                  "precision.", m_filename);

        Layout layout =
            (Layout) (flags & (uint32_t) SerializedFlags::LayoutMask);
        bool normals = has_flag(layout, Layout::Normals);

        uint64_t vertex_count, face_count, position_count, normal_count;
        stream->read(vertex_count);
        stream->read(face_count);
        stream->read(position_count);
        stream->read(normal_count);

        if (position_count > vertex_count ||
            normal_count > vertex_count ||
            (has_flag(layout, Layout::Tangents) && !normals))
            Throw("\"%s\": invalid serialized mesh header.", m_filename);

        PackedMesh pm(dr::backend_v<Float>, vertex_count, face_count,
                      layout, position_count, normal_count);

        stream->read_array(pm.vertices.data(),
                           vertex_count * MeshVertexStride);
        stream->read_array(pm.faces.data(), face_count * MeshFaceStride);
        if (position_count)
            stream->read_array(pm.position_index.data(), vertex_count);
        if (normal_count)
            stream->read_array(pm.normal_index.data(), vertex_count);

        // Custom attributes stream directly into the staging storage
        uint32_t attr_count = 0;
        stream->read(attr_count);
        for (uint32_t i = 0; i < attr_count; ++i) {
            std::string name;
            stream->read(name);
            uint8_t aflags = 0;
            stream->read(aflags);
            bool coeffs = (aflags & 1) != 0;
            uint32_t dim = 0;
            stream->read(dim);
            if (coeffs && !is_spectral_v<Spectrum>)
                Log(Warn, "\"%s\": attribute \"%s\" stores spectral "
                    "upsampling coefficients; a non-spectral variant "
                    "cannot reproduce the original colors.",
                    m_filename, name);
            float *dst = pm.add_attribute(name, dim, /* upsample_srgb */ !coeffs);
            stream->read_array(dst, (string::starts_with(name, "face_")
                                         ? face_count : vertex_count) * dim);
        }

        stream_->close();

        // The records arrived in bulk, so they are transformed after the fact
        pm.set_transform(m_to_world.scalar(), m_flip_normals);
        m_flip_normals = false;
        m_to_world = ScalarAffineTransform4f();
        pm.transform_records();

        // The stored FaceNormals flag applies when the scene description
        // leaves the property unset
        if ((flags & (uint32_t) SerializedFlags::FaceNormals) &&
            !props.has_property("face_normals"))
            m_face_normals = true;

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
