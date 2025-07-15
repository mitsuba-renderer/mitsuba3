#include <mitsuba/render/mesh.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/profiler.h>
#include <drjit-core/half.h>
#include <unordered_map>
#include <unordered_set>
#include <fstream>

#include "ply.h"

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-ply:

PLY (Stanford Triangle Format) mesh loader (:monosp:`ply`)
----------------------------------------------------------

.. pluginparameters::
 :extra-rows: 4

 * - filename
   - |string|
   - Filename of the PLY file that should be loaded

 * - face_normals
   - |bool|
   - When set to |true|, any existing or computed vertex normals are
     discarded and *face normals* will instead be used during rendering.
     This gives the rendered object a faceted appearance. (Default: |false|)

 * - flip_tex_coords
   - |bool|
   - Treat the vertical component of the texture as inverted? (Default: |false|)

 * - flip_normals
   - |bool|
   - Is the mesh inverted, i.e. should the normal vectors be flipped? (Default:|false|, i.e.
     the normals point outside)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none, i.e. object space = world space)

 * - vertex_count
   - |int|
   - Total number of vertices
   - |exposed|

 * - face_count
   - |int|
   - Total number of faces
   - |exposed|

 * - faces
   - :paramtype:`uint32[]`
   - Face indices buffer (flatten)
   - |exposed|

 * - vertex_positions
   - :paramtype:`float[]`
   - Vertex positions buffer (flatten) pre-multiplied by the object-to-world transformation.
   - |exposed|, |differentiable|, |discontinuous|

 * - vertex_normals
   - :paramtype:`float[]`
   - Vertex normals buffer (flatten)  pre-multiplied by the object-to-world transformation.
   - |exposed|, |differentiable|, |discontinuous|

 * - vertex_texcoords
   - :paramtype:`float[]`
   - Vertex texcoords buffer (flatten)
   - |exposed|, |differentiable|

 * - (Mesh attribute)
   - :paramtype:`float[]`
   - Mesh attribute buffer (flatten)
   - |exposed|, |differentiable|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_ply_bunny.jpg
   :caption: The Stanford bunny loaded with :monosp:`face_normals=false`.
.. subfigure:: ../../resources/data/docs/images/render/shape_ply_bunny_facet.jpg
   :caption: The Stanford bunny loaded with :monosp:`face_normals=true`. Note the faceted appearance.
.. subfigend::
   :label: fig-ply

This plugin implements a fast loader for the Stanford PLY format (both the
ASCII and binary format, which is preferred for performance reasons). The
current plugin implementation supports triangle meshes with optional UV
coordinates, vertex normals and other custom vertex or face attributes.

Consecutive attributes with names sharing a common prefix and using one of the following schemes:

``{prefix}_{x|y|z|w}``, ``{prefix}_{r|g|b|a}``, ``{prefix}_{0|1|2|3}``, ``{prefix}_{1|2|3|4}``

will be group together under a single multidimensional attribute named ``{vertex|face}_{prefix}``.

RGB color attributes can also be defined without a prefix, following the naming scheme ``{r|g|b|a}``
or ``{red|green|blue|alpha}``. Those attributes will be group together under a single
multidimensional attribute named ``{vertex|face}_color``.

.. tabs::
    .. code-tab:: xml
        :name: ply

        <shape type="ply">
            <string name="filename" value="my_shape.ply"/>
            <boolean name="flip_normals" value="true"/>
        </shape>

    .. code-tab:: python

        'type': 'ply',
        'filename': 'my_shape.ply',
        'flip_normals': True

.. note::

    Values stored in a RBG color attribute will automatically be converted into spectral model
    coefficients when using a spectral variant of the renderer.
 */

template <typename Float, typename Spectrum>
class PLYMesh final : public Mesh<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Mesh, m_name, m_bbox, m_to_world, m_vertex_count,
                   m_face_count, m_vertex_positions, m_vertex_normals,
                   m_vertex_texcoords, m_faces, add_attribute,
                   m_face_normals, has_vertex_normals,
                   has_vertex_texcoords, recompute_vertex_normals,
                   initialize)
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using ScalarIndex3 = dr::Array<ScalarIndex, 3>;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f ;
    using typename Base::InputVector2f;
    using typename Base::InputVector3f;
    using typename Base::InputNormal3f;
    using typename Base::FloatStorage;

    PLYMesh(const Properties &props) : Base(props) {
        /// Process vertex/index records in large batches
        constexpr size_t elements_per_packet = 1024;

        /* Causes all texture coordinates to be vertically flipped. */
        bool flip_tex_coords = props.get<bool>("flip_tex_coords", false);

        auto fs = file_resolver();
        fs::path file_path = fs->resolve(props.get<std::string_view>("filename"));
        m_name = file_path.filename().string();

        auto fail = [&](const char *descr) {
            Throw("Error while loading PLY file \"%s\": %s!", m_name, descr);
        };

        Log(Debug, "Loading mesh from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        ref<Stream> stream = new FileStream(file_path);
        ScopedPhase phase(ProfilerPhase::LoadGeometry);
        Timer timer;

        PLYHeader header;
        try {
            header = parse_ply_header(stream, m_name);
            if (header.ascii) {
                if (stream->size() > 100 * 1024)
                    Log(Warn,
                        "\"%s\": performance warning -- this file uses the ASCII PLY format, which "
                        "is slow to parse. Consider converting it to the binary PLY format.",
                        m_name);
                stream = parse_ascii((FileStream *) stream.get(), header.elements, m_name);
            }
        } catch (const std::exception &e) {
            fail(e.what());
        }

        bool has_vertex_normals = false;
        bool has_vertex_texcoords = false;

        ref<Struct> vertex_struct = new Struct();
        ref<Struct> face_struct = new Struct();

        for (auto &el : header.elements) {
            if (el.name == "vertex") {
                for (auto name : { "x", "y", "z" })
                    vertex_struct->append(name, struct_type_v<InputFloat>);

                if (!m_face_normals) {
                    for (auto name : { "nx", "ny", "nz" })
                        vertex_struct->append(name, struct_type_v<InputFloat>,
                                                +Struct::Flags::Default, 0.0);

                    if (el.struct_->has_field("nx") &&
                        el.struct_->has_field("ny") &&
                        el.struct_->has_field("nz"))
                        has_vertex_normals = true;
                }

                if (el.struct_->has_field("u") && el.struct_->has_field("v")) {
                    /* all good */
                } else if (el.struct_->has_field("texture_u") &&
                           el.struct_->has_field("texture_v")) {
                    el.struct_->field("texture_u").name = "u";
                    el.struct_->field("texture_v").name = "v";
                } else if (el.struct_->has_field("s") &&
                           el.struct_->has_field("t")) {
                    el.struct_->field("s").name = "u";
                    el.struct_->field("t").name = "v";
                }
                if (el.struct_->has_field("u") && el.struct_->has_field("v")) {
                    for (auto name : { "u", "v" })
                        vertex_struct->append(name, struct_type_v<InputFloat>);
                    has_vertex_texcoords = true;
                }

                // Look for other fields
                std::unordered_set<std::string> reserved_names = {
                    "x", "y", "z", "nx", "ny", "nz", "u", "v"
                };
                std::vector<PLYAttributeDescriptor> vertex_attributes_descriptors;
                find_other_fields("vertex_", vertex_attributes_descriptors,
                                  vertex_struct, el.struct_, reserved_names, m_name);

                size_t i_struct_size = el.struct_->size();
                size_t o_struct_size = vertex_struct->size();

                ref<StructConverter> conv;
                try {
                    conv = new StructConverter(el.struct_, vertex_struct);
                } catch (const std::exception &e) {
                    fail(e.what());
                }

                m_vertex_count = (ScalarSize) el.count;

                for (auto& descr: vertex_attributes_descriptors)
                    descr.buf.resize(m_vertex_count * descr.dim);

                std::unique_ptr<float[]> vertex_positions(new float[m_vertex_count * 3]);
                std::unique_ptr<float[]> vertex_normals(new float[m_vertex_count * 3]);
                std::unique_ptr<float[]> vertex_texcoords(new float[m_vertex_count * 2]);

                InputFloat* position_ptr = vertex_positions.get();
                InputFloat *normal_ptr   = vertex_normals.get();
                InputFloat *texcoord_ptr = vertex_texcoords.get();

                size_t packet_count     = el.count / elements_per_packet;
                size_t remainder_count  = el.count % elements_per_packet;
                size_t i_packet_size    = i_struct_size * elements_per_packet;
                size_t i_remainder_size = i_struct_size * remainder_count;
                size_t o_packet_size    = o_struct_size * elements_per_packet;

                std::unique_ptr<uint8_t[]> buf(new uint8_t[i_packet_size]);
                std::unique_ptr<uint8_t[]> buf_o(new uint8_t[o_packet_size]);

                for (size_t i = 0; i <= packet_count; ++i) {
                    uint8_t *target = (uint8_t *) buf_o.get();
                    size_t psize = (i != packet_count) ? i_packet_size : i_remainder_size;
                    size_t count = (i != packet_count) ? elements_per_packet : remainder_count;
                    stream->read(buf.get(), psize);
                    if (unlikely(!conv->convert(count, buf.get(), buf_o.get())))
                        fail("incompatible contents -- is this a triangle mesh?");

                    for (size_t j = 0; j < count; ++j) {
                        InputPoint3f p = dr::load<InputPoint3f>(target);
                        p = m_to_world.scalar() * p;
                        if (unlikely(!all(dr::isfinite(p))))
                            fail("mesh contains invalid vertex position data");
                        m_bbox.expand(p);
                        dr::store(position_ptr, p);
                        position_ptr += 3;

                        if (has_vertex_normals) {
                            InputNormal3f n = dr::load<InputNormal3f>(
                                target + sizeof(InputFloat) * 3);
                            n = dr::normalize(m_to_world.scalar() * n);
                            dr::store(normal_ptr, n);
                            normal_ptr += 3;
                        }

                        if (has_vertex_texcoords) {
                            InputVector2f uv = dr::load<InputVector2f>(
                                target + (m_face_normals
                                              ? sizeof(InputFloat) * 3
                                              : sizeof(InputFloat) * 6));
                            if (flip_tex_coords)
                                uv.y() = 1.f - uv.y();
                            dr::store(texcoord_ptr, uv);
                            texcoord_ptr += 2;
                        }

                        size_t target_offset =
                            sizeof(InputFloat) *
                            (!m_face_normals
                                 ? (has_vertex_texcoords ? 8 : 6)
                                 : (has_vertex_texcoords ? 5 : 3));

                        for (size_t k = 0; k < vertex_attributes_descriptors.size(); ++k) {
                            auto& descr = vertex_attributes_descriptors[k];
                            memcpy(descr.buf.data() + (i * elements_per_packet + j) * descr.dim,
                                   target + target_offset,
                                   descr.dim * sizeof(InputFloat));
                            target_offset += descr.dim * sizeof(InputFloat);
                        }

                        target += o_struct_size;
                    }
                }

                for (auto& descr: vertex_attributes_descriptors)
                    add_attribute(descr.name, descr.dim, descr.buf);

                m_vertex_positions = dr::load<FloatStorage>(vertex_positions.get(), m_vertex_count * 3);
                if (!m_face_normals)
                    m_vertex_normals = dr::load<FloatStorage>(vertex_normals.get(), m_vertex_count * 3);
                if (has_vertex_texcoords)
                    m_vertex_texcoords = dr::load<FloatStorage>(vertex_texcoords.get(), m_vertex_count * 2);

            } else if (el.name == "face") {
                std::string field_name;
                if (el.struct_->has_field("vertex_index.count"))
                    field_name = "vertex_index";
                else if (el.struct_->has_field("vertex_indices.count"))
                    field_name = "vertex_indices";
                else
                    fail("vertex_index/vertex_indices property not found");

                for (size_t i = 0; i < 3; ++i)
                    face_struct->append(tfm::format("i%i", i), struct_type_v<ScalarIndex>);

                // Look for other fields
                std::unordered_set<std::string> reserved_names = {
                    "vertex_index.count",
                    "vertex_indices.count",
                    "i0", "i1", "i2"
                };
                std::vector<PLYAttributeDescriptor> face_attributes_descriptors;
                find_other_fields("face_", face_attributes_descriptors,
                                  face_struct, el.struct_, reserved_names, m_name);

                size_t i_struct_size = el.struct_->size();
                size_t o_struct_size = face_struct->size();

                ref<StructConverter> conv;
                try {
                    conv = new StructConverter(el.struct_, face_struct);
                } catch (const std::exception &e) {
                    fail(e.what());
                }

                m_face_count = (ScalarSize) el.count;

                for (auto& descr: face_attributes_descriptors)
                    descr.buf.resize(m_face_count * descr.dim);

                std::unique_ptr<uint32_t[]> faces(new uint32_t[m_face_count * 3]);
                ScalarIndex* face_ptr = faces.get();

                size_t packet_count     = el.count / elements_per_packet;
                size_t remainder_count  = el.count % elements_per_packet;
                size_t i_packet_size    = i_struct_size * elements_per_packet;
                size_t i_remainder_size = i_struct_size * remainder_count;
                size_t o_packet_size    = o_struct_size * elements_per_packet;

                std::unique_ptr<uint8_t[]> buf(new uint8_t[i_packet_size]);
                std::unique_ptr<uint8_t[]> buf_o(new uint8_t[o_packet_size]);

                for (size_t i = 0; i <= packet_count; ++i) {
                    uint8_t *target = (uint8_t *) buf_o.get();
                    size_t psize = (i != packet_count) ? i_packet_size : i_remainder_size;
                    size_t count = (i != packet_count) ? elements_per_packet : remainder_count;

                    stream->read(buf.get(), psize);
                    if (unlikely(!conv->convert(count, buf.get(), buf_o.get())))
                        fail("incompatible contents -- is this a triangle mesh?");

                    for (size_t j = 0; j < count; ++j) {
                        ScalarIndex3 fi = dr::load<ScalarIndex3>(target);
                        dr::store(face_ptr, fi);
                        face_ptr += 3;

                        size_t target_offset = sizeof(InputFloat) * 3;
                        for (size_t k = 0; k < face_attributes_descriptors.size(); ++k) {
                            auto& descr = face_attributes_descriptors[k];
                            memcpy(descr.buf.data() + (i * elements_per_packet + j) * descr.dim,
                                   target + target_offset,
                                   descr.dim * sizeof(InputFloat));
                            target_offset += descr.dim * sizeof(InputFloat);
                        }

                        target += o_struct_size;
                    }
                }

                for (auto& descr: face_attributes_descriptors)
                    add_attribute(descr.name, descr.dim, descr.buf);

                m_faces = dr::load<DynamicBuffer<UInt32>>(faces.get(), m_face_count * 3);
            } else {
                Log(Warn, "\"%s\": skipping unknown element \"%s\"", m_name, el.name);
                stream->seek(stream->tell() + el.struct_->size() * el.count);
            }
        }

        if (stream->tell() != stream->size())
            fail("invalid file -- trailing content");

        Log(Debug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_name, m_face_count, m_vertex_count,
            util::mem_string(m_face_count * face_struct->size() +
                             m_vertex_count * vertex_struct->size()),
            util::time_string((float) timer.value())
        );

        if (!m_face_normals && !has_vertex_normals) {
            Timer timer2;
            recompute_vertex_normals();
            Log(Debug, "\"%s\": computed vertex normals (took %s)", m_name,
                util::time_string((float) timer2.value()));
        }

        initialize();
    }

private:
    MI_DECLARE_CLASS(PLYMesh)
};

MI_EXPORT_PLUGIN(PLYMesh)
NAMESPACE_END(mitsuba)
