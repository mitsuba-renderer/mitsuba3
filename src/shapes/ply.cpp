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

In addition, this plugin exposes the standard mesh state parameters
documented in :ref:`sec-shape-mesh-parameters`.

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
    MI_IMPORT_BASE(Mesh, m_filename, m_source_path, m_to_world,
                   m_vertex_count, m_face_count, m_flip_normals,
                   has_face_normals, from_packed, to_world_scalar)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using ScalarIndex3 = dr::Array<ScalarIndex, 3>;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f ;
    using typename Base::InputVector2f;
    using typename Base::InputNormal3f;

    PLYMesh(const Properties &props) : Base(props) {
        /// Process vertex/index records in large batches
        constexpr size_t elements_per_packet = 1024;

        // Causes all texture coordinates to be vertically flipped.
        bool flip_tex_coords = props.get<bool>("flip_tex_coords", false);

        auto fail = [&](const char *descr) {
            Throw("Error while loading PLY file \"%s\": %s!", m_filename, descr);
        };

        Log(Debug, "Loading mesh from \"%s\" ..", m_filename);

        ref<FileStream> file = new FileStream(m_source_path);
        ref<Stream> stream = file.get();
        ScopedPhase phase(ProfilerPhase::LoadGeometry);
        Timer timer;

        PLYHeader header;
        try {
            header = parse_ply_header(stream, m_filename);
            if (header.ascii) {
                if (stream->size() > 100 * 1024)
                    Log(Warn,
                        "\"%s\": performance warning -- this file uses the ASCII PLY format, which "
                        "is slow to parse. Consider converting it to the binary PLY format.",
                        m_filename);
                stream = parse_ascii(file.get(), header.elements, m_filename);
            }
        } catch (const std::exception &e) {
            fail(e.what());
        }

        // The element counts and the record layout follow from the header,
        // so the packed staging storage can be allocated up front and each
        // record written exactly once, at its final place.
        auto has_uv_fields = [](const sj::Struct &s) {
            return (s.contains("u") && s.contains("v")) ||
                   (s.contains("texture_u") && s.contains("texture_v")) ||
                   (s.contains("s") && s.contains("t"));
        };

        size_t vertex_count = 0, face_count = 0;
        bool has_normals = false, has_texcoords = false;
        for (auto &el : header.elements) {
            if (el.name == "vertex") {
                vertex_count = el.count;
                has_normals = !has_face_normals() &&
                              el.struct_.contains("nx") &&
                              el.struct_.contains("ny") &&
                              el.struct_.contains("nz");
                has_texcoords = has_uv_fields(el.struct_);
            } else if (el.name == "face") {
                face_count = el.count;
            }
        }

        PackedMesh pm(dr::backend_v<Float>, vertex_count, face_count,
                      make_layout(has_normals, has_texcoords));
        pm.set_transform(to_world_scalar(), m_flip_normals);
        m_flip_normals = false;
        m_to_world = new AnimatedTransform4f(ScalarAffineTransform4f());

        // Shared packet-streaming scaffolding of both element types:
        // convert the element's records into 'out_struct' in batches and
        // hand a pointer to each converted record to 'per_record'.
        auto stream_records = [&](const PLYElement &el,
                                  const sj::Struct &out_struct,
                                  auto &&per_record) {
            size_t i_struct_size = el.struct_.nbytes();
            size_t o_struct_size = out_struct.nbytes();

            const sj::Converter *conv = nullptr;
            try {
                conv = &sj::make_converter(el.struct_, out_struct);
            } catch (const std::exception &e) {
                fail(e.what());
            }

            size_t packet_count     = el.count / elements_per_packet;
            size_t remainder_count  = el.count % elements_per_packet;
            size_t i_packet_size    = i_struct_size * elements_per_packet;
            size_t i_remainder_size = i_struct_size * remainder_count;
            size_t o_packet_size    = o_struct_size * elements_per_packet;

            std::unique_ptr<uint8_t[]> buf(new uint8_t[i_packet_size]);
            std::unique_ptr<uint8_t[]> buf_o(new uint8_t[o_packet_size]);

            for (size_t i = 0; i <= packet_count; ++i) {
                const uint8_t *target = buf_o.get();
                size_t psize = (i != packet_count) ? i_packet_size : i_remainder_size;
                size_t count = (i != packet_count) ? elements_per_packet : remainder_count;
                stream->read(buf.get(), psize);
                if (unlikely(!conv->convert(buf.get(), buf_o.get(), count, 1)))
                    fail("incompatible contents -- is this a triangle mesh?");

                for (size_t j = 0; j < count; ++j) {
                    per_record(i * elements_per_packet + j, target);
                    target += o_struct_size;
                }
            }
        };

        // Copy the trailing attribute fields of one converted record into
        // the per-attribute staging buffers
        auto copy_attrs = [](const std::vector<PLYAttributeDescriptor> &descriptors,
                             const std::vector<float *> &attr_ptrs,
                             size_t index, const uint8_t *src) {
            for (size_t k = 0; k < descriptors.size(); ++k) {
                size_t dim = descriptors[k].dim;
                memcpy(attr_ptrs[k] + index * dim, src,
                       dim * sizeof(InputFloat));
                src += dim * sizeof(InputFloat);
            }
        };

        for (auto &el : header.elements) {
            if (el.name == "vertex") {
                sj::Struct vertex_struct;
                for (auto name : { "x", "y", "z" })
                    vertex_struct.append(name, struct_type_v<InputFloat>);

                if (has_normals)
                    for (auto name : { "nx", "ny", "nz" })
                        vertex_struct.append(name, struct_type_v<InputFloat>);

                if (el.struct_.contains("texture_u") &&
                    el.struct_.contains("texture_v")) {
                    el.struct_["texture_u"].name = "u";
                    el.struct_["texture_v"].name = "v";
                } else if (el.struct_.contains("s") &&
                           el.struct_.contains("t")) {
                    el.struct_["s"].name = "u";
                    el.struct_["t"].name = "v";
                }
                if (has_texcoords)
                    for (auto name : { "u", "v" })
                        vertex_struct.append(name, struct_type_v<InputFloat>);

                // Look for other fields
                std::unordered_set<std::string> reserved_names = {
                    "x", "y", "z", "nx", "ny", "nz", "u", "v"
                };
                std::vector<PLYAttributeDescriptor> descriptors;
                find_other_fields("vertex_", descriptors, vertex_struct,
                                  el.struct_, reserved_names, m_filename);

                std::vector<float *> attr_ptrs;
                for (auto &descr : descriptors)
                    attr_ptrs.push_back(pm.add_attribute(descr.name,
                                                         descr.dim));

                stream_records(el, vertex_struct,
                               [&](size_t index, const uint8_t *target) {
                    size_t offset = sizeof(InputFloat) * 3;

                    InputPoint3f p = dr::load<InputPoint3f>(target);
                    if (unlikely(!all(dr::isfinite(p))))
                        fail("mesh contains invalid vertex position data");

                    InputNormal3f n(0.f, 0.f, 0.f);
                    if (has_normals) {
                        n = dr::load<InputNormal3f>(target + offset);
                        offset += sizeof(InputFloat) * 3;
                    }

                    InputVector2f uv(0.f, 0.f);
                    if (has_texcoords) {
                        uv = dr::load<InputVector2f>(target + offset);
                        if (flip_tex_coords)
                            uv.y() = 1.f - uv.y();
                        offset += sizeof(InputFloat) * 2;
                    }

                    pm.set_vertex(index, p, n, uv);
                    copy_attrs(descriptors, attr_ptrs, index, target + offset);
                });
            } else if (el.name == "face") {
                if (!el.struct_.contains("vertex_index.count") &&
                    !el.struct_.contains("vertex_indices.count"))
                    fail("vertex_index/vertex_indices property not found");

                sj::Struct face_struct;
                for (size_t i = 0; i < 3; ++i)
                    face_struct.append(tfm::format("i%i", i), struct_type_v<ScalarIndex>);

                // Look for other fields
                std::unordered_set<std::string> reserved_names = {
                    "vertex_index.count",
                    "vertex_indices.count",
                    "i0", "i1", "i2"
                };
                std::vector<PLYAttributeDescriptor> descriptors;
                find_other_fields("face_", descriptors, face_struct,
                                  el.struct_, reserved_names, m_filename);

                std::vector<float *> attr_ptrs;
                for (auto &descr : descriptors)
                    attr_ptrs.push_back(pm.add_attribute(descr.name,
                                                         descr.dim));

                stream_records(el, face_struct,
                               [&](size_t index, const uint8_t *target) {
                    ScalarIndex3 fi = dr::load<ScalarIndex3>(target);
                    pm.set_face(index, { fi[0], fi[1], fi[2] });
                    copy_attrs(descriptors, attr_ptrs, index,
                               target + sizeof(ScalarIndex) * 3);
                });
            } else {
                Log(Warn, "\"%s\": skipping unknown element \"%s\"", m_filename, el.name);
                stream->seek(stream->tell() + el.struct_.nbytes() * el.count);
            }
        }

        if (stream->tell() != stream->size())
            fail("invalid file -- trailing content");

        file->close();
        from_packed(std::move(pm));

        Log(Debug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_filename, m_face_count, m_vertex_count,
            util::mem_string(
                (m_face_count * MeshFaceStride +
                 m_vertex_count * MeshVertexStride) * sizeof(uint32_t)),
            util::time_string((float) timer.value())
        );
    }

private:
    MI_DECLARE_CLASS(PLYMesh)
};

MI_EXPORT_PLUGIN(PLYMesh)
NAMESPACE_END(mitsuba)
