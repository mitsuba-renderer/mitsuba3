#include <mitsuba/render/mesh.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/timer.h>
#include <enoki/half.h>
#include <unordered_map>
#include <unordered_set>
#include <fstream>

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
 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation.
     (Default: none, i.e. object space = world space)

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

will be group together under a single multidimentional attribute named ``{vertex|face}_{prefix}``.

RGB color attributes can also be defined without a prefix, following the naming scheme ``{r|g|b|a}``
or ``{red|green|blue|alpha}``. Those attributes will be group together under a single
multidimentional attribute named ``{vertex|face}_color``.

.. note::

    Values stored in a RBG color attribute will automatically be converted into spectal model
    coefficients when using a spectral variant of the renderer.
 */

template <typename Float, typename Spectrum>
class PLYMesh final : public Mesh<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Mesh, m_name, m_bbox, m_to_world, m_vertex_count, m_face_count,
                    m_vertex_positions_buf, m_vertex_normals_buf, m_vertex_texcoords_buf,
                    m_faces_buf, add_attribute, m_disable_vertex_normals, has_vertex_normals,
                    has_vertex_texcoords, recompute_vertex_normals, set_children)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using ScalarIndex3 = Array<ScalarIndex, 3>;
    using typename Base::InputFloat;
    using typename Base::InputPoint3f ;
    using typename Base::InputVector2f;
    using typename Base::InputVector3f;
    using typename Base::InputNormal3f;
    using typename Base::FloatStorage;

    struct PLYElement {
        std::string name;
        size_t count;
        ref<Struct> struct_;
    };

    struct PLYHeader {
        bool ascii = false;
        std::vector<std::string> comments;
        std::vector<PLYElement> elements;
    };

    struct PLYAttributeDescriptor {
        std::string name;
        size_t dim;
        FloatStorage buf;
    };

    PLYMesh(const Properties &props) : Base(props) {
        /// Process vertex/index records in large batches
        constexpr size_t elements_per_packet = 1024;

        auto fs = Thread::thread()->file_resolver();
        fs::path file_path = fs->resolve(props.string("filename"));
        m_name = file_path.filename().string();

        auto fail = [&](const char *descr) {
            Throw("Error while loading PLY file \"%s\": %s!", m_name, descr);
        };

        Log(Debug, "Loading mesh from \"%s\" ..", m_name);
        if (!fs::exists(file_path))
            fail("file not found");

        ref<Stream> stream = new FileStream(file_path);
        Timer timer;

        PLYHeader header;
        try {
            header = parse_ply_header(stream);
            if (header.ascii) {
                if (stream->size() > 100 * 1024)
                    Log(Warn,
                        "\"%s\": performance warning -- this file uses the ASCII PLY format, which "
                        "is slow to parse. Consider converting it to the binary PLY format.",
                        m_name);
                stream = parse_ascii((FileStream *) stream.get(), header.elements);
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

                if (!m_disable_vertex_normals) {
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
                                  vertex_struct, el.struct_, reserved_names);

                size_t i_struct_size = el.struct_->size();
                size_t o_struct_size = vertex_struct->size();

                ref<StructConverter> conv;
                try {
                    conv = new StructConverter(el.struct_, vertex_struct);
                } catch (const std::exception &e) {
                    fail(e.what());
                }

                m_vertex_count = (ScalarSize) el.count;
                m_vertex_positions_buf = empty<FloatStorage>(m_vertex_count * 3);
                if (!m_disable_vertex_normals)
                    m_vertex_normals_buf = empty<FloatStorage>(m_vertex_count * 3);
                if (has_vertex_texcoords)
                    m_vertex_texcoords_buf = empty<FloatStorage>(m_vertex_count * 2);

                for (auto& descr: vertex_attributes_descriptors) {
                    descr.buf = empty<FloatStorage>(m_vertex_count * descr.dim);
                    descr.buf.managed();
                }

                m_vertex_positions_buf.managed();
                m_vertex_normals_buf.managed();
                m_vertex_texcoords_buf.managed();

                if constexpr (is_cuda_array_v<Float>)
                    cuda_sync();

                size_t packet_count     = el.count / elements_per_packet;
                size_t remainder_count  = el.count % elements_per_packet;
                size_t i_packet_size    = i_struct_size * elements_per_packet;
                size_t i_remainder_size = i_struct_size * remainder_count;
                size_t o_packet_size    = o_struct_size * elements_per_packet;

                std::unique_ptr<uint8_t[]> buf(new uint8_t[i_packet_size]);
                std::unique_ptr<uint8_t[]> buf_o(new uint8_t[o_packet_size]);

                InputFloat* position_ptr = m_vertex_positions_buf.data();
                InputFloat* normal_ptr   = m_vertex_normals_buf.data();
                InputFloat* texcoord_ptr = m_vertex_texcoords_buf.data();

                for (size_t i = 0; i <= packet_count; ++i) {
                    uint8_t *target = (uint8_t *) buf_o.get();
                    size_t psize = (i != packet_count) ? i_packet_size : i_remainder_size;
                    size_t count = (i != packet_count) ? elements_per_packet : remainder_count;
                    stream->read(buf.get(), psize);
                    if (unlikely(!conv->convert(count, buf.get(), buf_o.get())))
                        fail("incompatible contents -- is this a triangle mesh?");

                    for (size_t j = 0; j < count; ++j) {
                        InputPoint3f p = enoki::load<InputPoint3f>(target);
                        p = m_to_world.transform_affine(p);
                        if (unlikely(!all(enoki::isfinite(p))))
                            fail("mesh contains invalid vertex positions/normal data");
                        m_bbox.expand(p);
                        store_unaligned(position_ptr, p);
                        position_ptr += 3;

                        if (has_vertex_normals) {
                            InputNormal3f n = enoki::load<InputNormal3f>(
                                target + sizeof(InputFloat) * 3);
                            n = normalize(m_to_world.transform_affine(n));
                            store_unaligned(normal_ptr, n);
                            normal_ptr += 3;
                        }

                        if (has_vertex_texcoords) {
                            InputVector2f uv = enoki::load<InputVector2f>(
                                target + (m_disable_vertex_normals
                                              ? sizeof(InputFloat) * 3
                                              : sizeof(InputFloat) * 6));
                            store_unaligned(texcoord_ptr, uv);
                            texcoord_ptr += 2;
                        }

                        size_t target_offset =
                            sizeof(InputFloat) *
                            (!m_disable_vertex_normals
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

                for (auto& descr: vertex_attributes_descriptors) {
                    add_attribute(descr.name, descr.dim, descr.buf);
                }
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
                                  face_struct, el.struct_, reserved_names);

                size_t i_struct_size = el.struct_->size();
                size_t o_struct_size = face_struct->size();

                ref<StructConverter> conv;
                try {
                    conv = new StructConverter(el.struct_, face_struct);
                } catch (const std::exception &e) {
                    fail(e.what());
                }

                m_face_count = (ScalarSize) el.count;
                m_faces_buf = empty<DynamicBuffer<UInt32>>(m_face_count * 3);
                m_faces_buf.managed();

                for (auto& descr: face_attributes_descriptors) {
                    descr.buf = empty<FloatStorage>(m_face_count * descr.dim);
                    descr.buf.managed();
                }

                ScalarIndex* face_ptr = m_faces_buf.data();

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
                        ScalarIndex3 fi = enoki::load<ScalarIndex3>(target);
                        store_unaligned(face_ptr, fi);
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

                for (auto& descr: face_attributes_descriptors) {
                    add_attribute(descr.name, descr.dim, descr.buf);
                }
            } else {
                Log(Warn, "\"%s\": Skipping unknown element \"%s\"", m_name, el.name);
                stream->seek(stream->tell() + el.struct_->size() * el.count);
            }
        }

        if (stream->tell() != stream->size())
            fail("invalid file -- trailing content");

        Log(Debug, "\"%s\": read %i faces, %i vertices (%s in %s)",
            m_name, m_face_count, m_vertex_count,
            util::mem_string(m_face_count * face_struct->size() +
                             m_vertex_count * vertex_struct->size()),
            util::time_string(timer.value())
        );

        if (!m_disable_vertex_normals && !has_vertex_normals) {
            Timer timer2;
            recompute_vertex_normals();
            Log(Debug, "\"%s\": computed vertex normals (took %s)", m_name,
                util::time_string(timer2.value()));
        }

        set_children();
    }

private:
    PLYHeader parse_ply_header(Stream *stream) {
        Struct::ByteOrder byte_order = Struct::host_byte_order();
        bool ply_tag_seen = false;
        bool header_processed = false;
        PLYHeader header;

        std::unordered_map<std::string, Struct::Type> fmt_map;
        fmt_map["char"]   = Struct::Type::Int8;
        fmt_map["uchar"]  = Struct::Type::UInt8;
        fmt_map["short"]  = Struct::Type::Int16;
        fmt_map["ushort"] = Struct::Type::UInt16;
        fmt_map["int"]    = Struct::Type::Int32;
        fmt_map["uint"]   = Struct::Type::UInt32;
        fmt_map["float"]  = Struct::Type::Float32;
        fmt_map["double"] = Struct::Type::Float64;

        /* Unofficial extensions :) */
        fmt_map["uint8"]   = Struct::Type::UInt8;
        fmt_map["uint16"]  = Struct::Type::UInt16;
        fmt_map["uint32"]  = Struct::Type::UInt32;
        fmt_map["int8"]    = Struct::Type::Int8;
        fmt_map["int16"]   = Struct::Type::Int16;
        fmt_map["int32"]   = Struct::Type::Int32;
        fmt_map["long"]    = Struct::Type::Int64;
        fmt_map["ulong"]   = Struct::Type::UInt64;
        fmt_map["half"]    = Struct::Type::Float16;
        fmt_map["float16"] = Struct::Type::Float16;
        fmt_map["float32"] = Struct::Type::Float32;
        fmt_map["float64"] = Struct::Type::Float64;

        ref<Struct> struct_;

        while (true) {
            std::string line = stream->read_line();
            std::istringstream iss(line);
            std::string token;
            if (!(iss >> token))
                continue;

            if (token == "comment") {
                std::getline(iss, line);
                header.comments.push_back(string::trim(line));
                continue;
            } else if (token == "ply") {
                if (ply_tag_seen)
                    Throw("invalid PLY header: duplicate \"ply\" tag");
                ply_tag_seen = true;
                if (iss >> token)
                    Throw("invalid PLY header: excess tokens after \"ply\"");
            } else if (token == "format") {
                if (!ply_tag_seen)
                    Throw("invalid PLY header: \"format\" before \"ply\" tag");
                if (header_processed)
                    Throw("invalid PLY header: duplicate \"format\" tag");
                if (!(iss >> token))
                    Throw("invalid PLY header: missing token after \"format\"");
                if (token == "ascii")
                    header.ascii = true;
                else if (token == "binary_little_endian")
                    byte_order = Struct::ByteOrder::LittleEndian;
                else if (token == "binary_big_endian")
                    byte_order = Struct::ByteOrder::BigEndian;
                else
                    Throw("invalid PLY header: invalid token after \"format\"");
                if (!(iss >> token))
                    Throw("invalid PLY header: missing version number after \"format\"");
                if (token != "1.0")
                    Throw("PLY file has unknown version number \"%s\"", token);
                if (iss >> token)
                    Throw("invalid PLY header: excess tokens after \"format\"");
                header_processed = true;
            } else if (token == "element") {
                if (!(iss >> token))
                    Throw("invalid PLY header: missing token after \"element\"");
                header.elements.emplace_back();
                auto &element = header.elements.back();
                element.name = token;
                if (!(iss >> token))
                    Throw("invalid PLY header: missing token after \"element\"");
                element.count = (size_t) stoull(token);
                struct_ = element.struct_ = new Struct(true, byte_order);
            } else if (token == "property") {
                if (!header_processed)
                    Throw("invalid PLY header: encountered \"property\" before \"format\"");
                if (header.elements.empty())
                    Throw("invalid PLY header: encountered \"property\" before \"element\"");
                if (!(iss >> token))
                    Throw("invalid PLY header: missing token after \"property\"");

                if (token == "list") {
                    if (!(iss >> token))
                        Throw("invalid PLY header: missing token after \"property list\"");
                    auto it1 = fmt_map.find(token);
                    if (it1 == fmt_map.end())
                        Throw("invalid PLY header: unknown format type \"%s\"", token);

                    if (!(iss >> token))
                        Throw("invalid PLY header: missing token after \"property list\"");
                    auto it2 = fmt_map.find(token);
                    if (it2 == fmt_map.end())
                        Throw("invalid PLY header: unknown format type \"%s\"", token);

                    if (!(iss >> token))
                        Throw("invalid PLY header: missing token after \"property list\"");

                    struct_->append(token + ".count", it1->second, +Struct::Flags::Assert, 3);
                    for (int i = 0; i<3; ++i)
                        struct_->append(tfm::format("i%i", i), it2->second);
                } else {
                    auto it = fmt_map.find(token);
                    if (it == fmt_map.end())
                        Throw("invalid PLY header: unknown format type \"%s\"", token);
                    if (!(iss >> token))
                        Throw("invalid PLY header: missing token after \"property\"");
                    uint32_t flags = +Struct::Flags::None;
                    if (it->second >= Struct::Type::Int8 &&
                        it->second <= Struct::Type::UInt64)
                        flags = Struct::Flags::Normalized | Struct::Flags::Gamma;
                    struct_->append(token, it->second, flags);
                }

                if (iss >> token)
                    Throw("invalid PLY header: excess tokens after \"property\"");
            } else if (token == "end_header") {
                if (iss >> token)
                    Throw("invalid PLY header: excess tokens after \"end_header\"");
                break;
            } else {
                Throw("invalid PLY header: unknown token \"%s\"", token);
            }
        }
        if (!header_processed)
            Throw("invalid PLY file: no header information");
        return header;
    }

    ref<Stream> parse_ascii(FileStream *in, const std::vector<PLYElement> &elements) {
        ref<Stream> out = new MemoryStream();
        std::fstream &is = *in->native();
        for (auto const &el : elements) {
            for (size_t i = 0; i < el.count; ++i) {
                for (auto const &field : *(el.struct_)) {
                    switch (field.type) {
                        case Struct::Type::Int8: {
                                int value;
                                if (!(is >> value)) Throw("Could not parse \"char\" value for field %s", field.name);
                                if (value < -128 || value > 127)
                                    Throw("Could not parse \"char\" value for field %s", field.name);
                                out->write((int8_t) value);
                            }
                            break;

                        case Struct::Type::UInt8: {
                                int value;
                                if (!(is >> value))
                                    Throw("Could not parse \"uchar\" value for field %s (may be due to non-triangular faces)", field.name);
                                if (value < 0 || value > 255)
                                    Throw("Could not parse \"uchar\" value for field %s (may be due to non-triangular faces)", field.name);
                                out->write((uint8_t) value);
                            }
                            break;

                        case Struct::Type::Int16: {
                                int16_t value;
                                if (!(is >> value)) Throw("Could not parse \"short\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        case Struct::Type::UInt16: {
                                uint16_t value;
                                if (!(is >> value)) Throw("Could not parse \"ushort\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        case Struct::Type::Int32: {
                                int32_t value;
                                if (!(is >> value)) Throw("Could not parse \"int\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        case Struct::Type::UInt32: {
                                uint32_t value;
                                if (!(is >> value)) Throw("Could not parse \"uint\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        case Struct::Type::Int64: {
                                int64_t value;
                                if (!(is >> value)) Throw("Could not parse \"long\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        case Struct::Type::UInt64: {
                                uint64_t value;
                                if (!(is >> value)) Throw("Could not parse \"ulong\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        case Struct::Type::Float16: {
                                float value;
                                if (!(is >> value)) Throw("Could not parse \"half\" value for field %s", field.name);
                                out->write(enoki::half::float32_to_float16(value));
                            }
                            break;

                        case Struct::Type::Float32: {
                                float value;
                                if (!(is >> value)) Throw("Could not parse \"float\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        case Struct::Type::Float64: {
                                double value;
                                if (!(is >> value)) Throw("Could not parse \"double\" value for field %s", field.name);
                                out->write(value);
                            }
                            break;

                        default:
                            Throw("internal error");
                    }
                }
            }
        }
        std::string token;
        if (is >> token)
            Throw("Trailing tokens after end of PLY file");
        out->seek(0);
        return out;
    }

    void find_other_fields(const std::string& type, std::vector<PLYAttributeDescriptor> &vertex_attributes_descriptors, ref<Struct> target_struct,
        ref<Struct> ref_struct, std::unordered_set<std::string> &reserved_names) {

        if (ref_struct->has_field("r") && ref_struct->has_field("g") && ref_struct->has_field("b")) {
            /* all good */
        } else if (ref_struct->has_field("red") &&
                   ref_struct->has_field("green") &&
                   ref_struct->has_field("blue")) {
            ref_struct->field("red").name   = "r";
            ref_struct->field("green").name = "g";
            ref_struct->field("blue").name  = "b";
            if (ref_struct->has_field("alpha"))
                ref_struct->field("alpha").name = "a";
        }
        if (ref_struct->has_field("r") && ref_struct->has_field("g") && ref_struct->has_field("b")) {
            // vertex_attribute_structs.push_back(new Struct());
            size_t field_count = 3;
            for (auto name : { "r", "g", "b" })
                target_struct->append(name, struct_type_v<InputFloat>);
            if (ref_struct->has_field("a")) {
                target_struct->append("a", struct_type_v<InputFloat>);
                ++field_count;
            }
            vertex_attributes_descriptors.push_back({ type + "color", field_count, FloatStorage() });

            if (!ref_struct->field("r").is_float())
                Log(Warn, "Mesh attribute \"%s\" has integer fields: color attributes are expected to be in the [0, 1] range.",
                    (type + "color").c_str());
        }

        reserved_names.insert({ "r", "g", "b", "a" });

        // Check for any other fields.
        // Fields in the same attribute must be contiguous, have the same prefix, and postfix of the same category.
        // Valid categories are [x, y, z, w], [r, g, b, a], [0, 1, 2, 3], [1, 2, 3, 4]

        constexpr size_t valid_postfix_count = 4;
        const char* postfixes[4] = {
            "xr01",
            "yg12",
            "zb23",
            "wa34"
        };

        std::unordered_set<std::string> prefixes_encountered;
        std::string current_prefix = "";
        size_t current_postfix_index = 0;
        size_t current_postfix_level_index = 0;
        bool reading_attribute = false;
        Struct::Type current_type;

        auto ignore_attribute = [&]() {
            // Reset state
            current_prefix = "";
            current_postfix_index = 0;
            current_postfix_level_index = 0;
            reading_attribute = false;
        };
        auto flush_attribute = [&]() {
            if (current_postfix_level_index != 1 && current_postfix_level_index != 3) {
                Log(Warn, "Attribute must have either 1 or 3 fields (had %d) : attribute \"%s\" ignored",
                    current_postfix_level_index, (type + current_prefix).c_str());
                ignore_attribute();
                return;
            }

            if (!Struct::is_float(current_type) && current_postfix_level_index == 3)
                Log(Warn, "Attribute \"%s\" has integer fields: color attributes are expected to be in the [0, 1] range.",
                    (type + current_prefix).c_str());

            for(size_t i = 0; i < current_postfix_level_index; ++i)
                target_struct->append(current_prefix + "_" + postfixes[i][current_postfix_index],
                                      struct_type_v<InputFloat>);

            std::string color_postfix = (current_postfix_index == 1) ? "_color" : "";
            vertex_attributes_descriptors.push_back(
                { type + current_prefix + color_postfix,
                  current_postfix_level_index, FloatStorage() });

            prefixes_encountered.insert(current_prefix);
            // Reset state
            ignore_attribute();
        };

        size_t field_count = ref_struct->field_count();
        for (size_t i = 0; i < field_count; ++i) {
            const Struct::Field& field = ref_struct->operator[](i);
            if (reserved_names.find(field.name) != reserved_names.end())
                continue;

            current_type = field.type;

            auto pos = field.name.find_last_of('_');
            if (pos == std::string::npos) {
                Log(Warn, "Attribute without postifx are not handled for now: attribute \"%s\" ignored.", field.name.c_str());
                if (reading_attribute)
                    flush_attribute();
                continue; // Don't do anything with attributes without postfix (for now)
            }

            const std::string postfix = field.name.substr(pos+1);
            if (postfix.size() != 1) {
                Log(Warn, "Attribute postfix can only be one letter long.");
                if (reading_attribute)
                    flush_attribute();
                continue;
            }

            const std::string prefix = field.name.substr(0, pos);
            if (reading_attribute && prefix != current_prefix)
                flush_attribute();

            if (!reading_attribute && prefixes_encountered.find(prefix) != prefixes_encountered.end()) {
                Log(Warn, "Attribute prefix has already been encountered: attribute \"%s\" ignored.", field.name.c_str());
                while(i < field_count && ref_struct->operator[](i).name.find(prefix) == 0) ++i;
                if (i == field_count)
                    break;
                continue;
            }

            char chpostfix = postfix[0];
            // If this is the first occurence of this attribute, we look for the postfix index
            if (!reading_attribute) {
                int32_t postfix_index = -1;
                for (size_t j = 0; j < valid_postfix_count; ++j) {
                    if (chpostfix == postfixes[0][j]) {
                        postfix_index = (int32_t)j;
                        break;
                    }
                }
                if (postfix_index == -1) {
                    Log(Warn, "Attribute can't start with postfix %c.", chpostfix);
                    continue;
                }
                reading_attribute = true;
                current_postfix_index = postfix_index;
                current_prefix = prefix;
            } else { // otherwise the postfix sequence should follow the naming rules
                if (chpostfix != postfixes[current_postfix_level_index][current_postfix_index]) {
                    Log(Warn, "Attribute postfix sequence is invalid: attribute \"%s\" ignored.", current_prefix.c_str());
                    ignore_attribute();
                    while(i < field_count && ref_struct->operator[](i).name.find(prefix) == 0) ++i;
                    if (i == field_count)
                        break;
                }
            }
            // In both cases, we increment the postfix_level_index
            ++current_postfix_level_index;
        }
        if (reading_attribute)
            flush_attribute();
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(PLYMesh, Mesh)
MTS_EXPORT_PLUGIN(PLYMesh, "PLY Mesh")
NAMESPACE_END(mitsuba)
