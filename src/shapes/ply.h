#pragma once

#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <string>

#include <mitsuba/core/struct.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/mstream.h>
#include <drjit-core/half.h>

NAMESPACE_BEGIN(mitsuba)

using InputFloat = float;

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
    std::vector<InputFloat> buf;
};

PLYHeader parse_ply_header(Stream *stream, std::string name) {
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
                Throw("\"%s\": invalid PLY header: duplicate \"ply\" tag", name);
            ply_tag_seen = true;
            if (iss >> token)
                Throw("\"%s\": invalid PLY header: excess tokens after \"ply\"", name);
        } else if (token == "format") {
            if (!ply_tag_seen)
                Throw("\"%s\": invalid PLY header: \"format\" before \"ply\" tag", name);
            if (header_processed)
                Throw("\"%s\": invalid PLY header: duplicate \"format\" tag", name);
            if (!(iss >> token))
                Throw("\"%s\": invalid PLY header: missing token after \"format\"", name);
            if (token == "ascii")
                header.ascii = true;
            else if (token == "binary_little_endian")
                byte_order = Struct::ByteOrder::LittleEndian;
            else if (token == "binary_big_endian")
                byte_order = Struct::ByteOrder::BigEndian;
            else
                Throw("\"%s\": invalid PLY header: invalid token after \"format\"", name);
            if (!(iss >> token))
                Throw("\"%s\": invalid PLY header: missing version number after \"format\"", name);
            if (token != "1.0")
                Throw("\"%s\": PLY file has unknown version number \"%s\"", name, token);
            if (iss >> token)
                Throw("\"%s\": invalid PLY header: excess tokens after \"format\"", name);
            header_processed = true;
        } else if (token == "element") {
            if (!(iss >> token))
                Throw("\"%s\": invalid PLY header: missing token after \"element\"", name);
            header.elements.emplace_back();
            auto &element = header.elements.back();
            element.name = token;
            if (!(iss >> token))
                Throw("\"%s\": invalid PLY header: missing token after \"element\"", name);
            element.count = (size_t) stoull(token);
            struct_ = element.struct_ = new Struct(true, byte_order);
        } else if (token == "property") {
            if (!header_processed)
                Throw("\"%s\": invalid PLY header: encountered \"property\" before \"format\"", name);
            if (header.elements.empty())
                Throw("\"%s\": invalid PLY header: encountered \"property\" before \"element\"", name);
            if (!(iss >> token))
                Throw("\"%s\": invalid PLY header: missing token after \"property\"", name);

            if (token == "list") {
                if (!(iss >> token))
                    Throw("\"%s\": invalid PLY header: missing token after \"property list\"", name);
                auto it1 = fmt_map.find(token);
                if (it1 == fmt_map.end())
                    Throw("\"%s\": invalid PLY header: unknown format type \"%s\"", name, token);

                if (!(iss >> token))
                    Throw("\"%s\": invalid PLY header: missing token after \"property list\"", name);
                auto it2 = fmt_map.find(token);
                if (it2 == fmt_map.end())
                    Throw("\"%s\": invalid PLY header: unknown format type \"%s\"", name, token);

                if (!(iss >> token))
                    Throw("\"%s\": invalid PLY header: missing token after \"property list\"", name);

                struct_->append(token + ".count", it1->second, +Struct::Flags::Assert, 3);
                for (int i = 0; i<3; ++i)
                    struct_->append(tfm::format("i%i", i), it2->second);
            } else {
                auto it = fmt_map.find(token);
                if (it == fmt_map.end())
                    Throw("\"%s\": invalid PLY header: unknown format type \"%s\"", name, token);
                if (!(iss >> token))
                    Throw("\"%s\": invalid PLY header: missing token after \"property\"", name);
                uint32_t flags = +Struct::Flags::Empty;
                if (it->second >= Struct::Type::Int8 &&
                    it->second <= Struct::Type::UInt64)
                    flags = Struct::Flags::Normalized | Struct::Flags::Gamma;
                struct_->append(token, it->second, flags);
            }

            if (iss >> token)
                Throw("\"%s\": invalid PLY header: excess tokens after \"property\"", name);
        } else if (token == "end_header") {
            if (iss >> token)
                Throw("\"%s\": invalid PLY header: excess tokens after \"end_header\"", name);
            break;
        } else {
            Throw("\"%s\": invalid PLY header: unknown token \"%s\"", name, token);
        }
    }
    if (!header_processed)
        Throw("\"%s\": invalid PLY file: no header information", name);
    return header;
}

ref<Stream> parse_ascii(FileStream *in, const std::vector<PLYElement> &elements, std::string name) {
    ref<Stream> out = new MemoryStream();
    std::fstream &is = *in->native();
    for (auto const &el : elements) {
        for (size_t i = 0; i < el.count; ++i) {
            for (auto const &field : *(el.struct_)) {
                switch (field.type) {
                    case Struct::Type::Int8: {
                            int value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"char\" value for field %s", name, field.name);
                            if (value < -128 || value > 127)
                                Throw("\"%s\": could not parse \"char\" value for field %s", name, field.name);
                            out->write((int8_t) value);
                        }
                        break;

                    case Struct::Type::UInt8: {
                            int value;
                            if (!(is >> value))
                                Throw("\"%s\": could not parse \"uchar\" value for field %s (may be due to non-triangular faces)", name, field.name);
                            if (value < 0 || value > 255)
                                Throw("\"%s\": could not parse \"uchar\" value for field %s (may be due to non-triangular faces)", name, field.name);
                            out->write((uint8_t) value);
                        }
                        break;

                    case Struct::Type::Int16: {
                            int16_t value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"short\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    case Struct::Type::UInt16: {
                            uint16_t value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"ushort\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    case Struct::Type::Int32: {
                            int32_t value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"int\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    case Struct::Type::UInt32: {
                            uint32_t value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"uint\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    case Struct::Type::Int64: {
                            int64_t value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"long\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    case Struct::Type::UInt64: {
                            uint64_t value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"ulong\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    case Struct::Type::Float16: {
                            float value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"half\" value for field %s", name, field.name);
                            out->write(dr::half(value).value);
                        }
                        break;

                    case Struct::Type::Float32: {
                            float value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"float\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    case Struct::Type::Float64: {
                            double value;
                            if (!(is >> value)) Throw("\"%s\": could not parse \"double\" value for field %s", name, field.name);
                            out->write(value);
                        }
                        break;

                    default:
                        Throw("\"%s\": internal error", name);
                }
            }
        }
    }
    std::string token;
    if (is >> token)
        Throw("\"%s\": trailing tokens after end of PLY file", name);
    out->seek(0);
    return out;
}

void find_other_fields(const std::string& type, std::vector<PLYAttributeDescriptor> &vertex_attributes_descriptors, ref<Struct> target_struct,
    ref<Struct> ref_struct, std::unordered_set<std::string> &reserved_names, std::string name) {

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
        for (auto name2 : { "r", "g", "b" })
            target_struct->append(name2, struct_type_v<InputFloat>);
        if (ref_struct->has_field("a")) {
            target_struct->append("a", struct_type_v<InputFloat>);
            ++field_count;
        }
        vertex_attributes_descriptors.push_back({ type + "color", field_count, std::vector<InputFloat>() });

        if (!ref_struct->field("r").is_float())
            Log(Warn, "\"%s\": Mesh attribute \"%s\" has integer fields: color attributes are expected to be in the [0, 1] range.",
                name, (type + "color").c_str());
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
            Log(Warn, "\"%s\": attribute must have either 1 or 3 fields (had %d) : attribute \"%s\" ignored",
                name, current_postfix_level_index, (type + current_prefix).c_str());
            ignore_attribute();
            return;
        }

        if (!Struct::is_float(current_type) && current_postfix_level_index == 3)
            Log(Warn, "\"%s\": attribute \"%s\" has integer fields: color attributes are expected to be in the [0, 1] range.",
                name, (type + current_prefix).c_str());

        for(size_t i = 0; i < current_postfix_level_index; ++i)
            target_struct->append(current_prefix + "_" + postfixes[i][current_postfix_index],
                                    struct_type_v<InputFloat>);

        std::string color_postfix = (current_postfix_index == 1) ? "_color" : "";
        vertex_attributes_descriptors.push_back(
            { type + current_prefix + color_postfix,
                current_postfix_level_index, std::vector<InputFloat>() });

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
            Log(Warn, "\"%s\": attributes without postfix are not handled for now: attribute \"%s\" ignored.", name, field.name.c_str());
            if (reading_attribute)
                flush_attribute();
            continue; // Don't do anything with attributes without postfix (for now)
        }

        const std::string postfix = field.name.substr(pos+1);
        if (postfix.size() != 1) {
            Log(Warn, "\"%s\": attribute postfix can only be one letter long.");
            if (reading_attribute)
                flush_attribute();
            continue;
        }

        const std::string prefix = field.name.substr(0, pos);
        if (reading_attribute && prefix != current_prefix)
            flush_attribute();

        if (!reading_attribute && prefixes_encountered.find(prefix) != prefixes_encountered.end()) {
            Log(Warn, "\"%s\": attribute prefix has already been encountered: attribute \"%s\" ignored.", name, field.name.c_str());
            while(i < field_count && ref_struct->operator[](i).name.find(prefix) == 0) ++i;
            if (i == field_count)
                break;
            continue;
        }

        char chpostfix = postfix[0];
        // If this is the first occurrence of this attribute, we look for the postfix index
        if (!reading_attribute) {
            int32_t postfix_index = -1;
            for (size_t j = 0; j < valid_postfix_count; ++j) {
                if (chpostfix == postfixes[0][j]) {
                    postfix_index = (int32_t)j;
                    break;
                }
            }
            if (postfix_index == -1) {
                Log(Warn, "\"%s\": attribute can't start with postfix %c.", name, chpostfix);
                continue;
            }
            reading_attribute = true;
            current_postfix_index = postfix_index;
            current_prefix = prefix;
        } else { // otherwise the postfix sequence should follow the naming rules
            if (chpostfix != postfixes[current_postfix_level_index][current_postfix_index]) {
                Log(Warn, "\"%s\": attribute postfix sequence is invalid: attribute \"%s\" ignored.", name, current_prefix.c_str());
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

NAMESPACE_END(mitsuba)
