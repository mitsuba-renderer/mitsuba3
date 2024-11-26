#include <cctype>
#include <fstream>
#include <set>
#include <unordered_map>
#include <mutex>
#include <map>

#include <mitsuba/core/class.h>
#include <mitsuba/core/config.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/core/timer.h>
#include <pugixml.hpp>
#include <nanothread/nanothread.h>

/// Linux <sys/sysmacros.h> defines these as macros .. :(
#if defined(major)
#  undef major
#endif

#if defined(minor)
#  undef minor
#endif

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)

using Version = util::Version;

// Set of supported XML tags
enum class Tag {
    Boolean, Integer, Float, String, Point, Vector, Spectrum, RGB,
    Transform, Translate, Matrix, Rotate, Scale, LookAt, Object,
    NamedReference, Include, Alias, Default, Resource, Invalid
};

// Check if the name corresponds to an unbounded spectrum property which require
// special handling
bool is_unbounded_spectrum(const std::string &name) {
    return name == "eta" || name == "k" || name == "int_ior" || name == "ext_ior";
}

NAMESPACE_BEGIN(detail)

using Float = Properties::Float;
MI_IMPORT_CORE_TYPES()

static int64_t stoll(const std::string &s) {
    size_t offset = 0;
    int64_t result = std::stoll(s, &offset);

    for (size_t i = offset; i < s.size(); ++i) {
        if (!std::isspace(s[i]))
            Throw("Invalid trailing characters in integer \"%s\"", s);
    }

    return result;
}


static std::unordered_map<std::string, Tag> *tags = nullptr;
static std::unordered_map<std::string, // e.g. bsdf.scalar_rgb
                          const Class *> *tag_class = nullptr;

inline std::string class_key(const std::string &name, const std::string &variant) {
    return name + "." + variant;
}

// Called by Class::Class()
void register_class(const Class *class_) {
    Assert(class_ != nullptr);

    if (!tags) {
        tags = new std::unordered_map<std::string, Tag>();
        tag_class = new std::unordered_map<std::string, const Class *>();

        // Create an initial mapping of tag names to IDs
        (*tags)["boolean"]       = Tag::Boolean;
        (*tags)["integer"]       = Tag::Integer;
        (*tags)["float"]         = Tag::Float;
        (*tags)["string"]        = Tag::String;
        (*tags)["point"]         = Tag::Point;
        (*tags)["vector"]        = Tag::Vector;
        (*tags)["transform"]     = Tag::Transform;
        (*tags)["translate"]     = Tag::Translate;
        (*tags)["matrix"]        = Tag::Matrix;
        (*tags)["rotate"]        = Tag::Rotate;
        (*tags)["scale"]         = Tag::Scale;
        (*tags)["lookat"]        = Tag::LookAt;
        (*tags)["ref"]           = Tag::NamedReference;
        (*tags)["spectrum"]      = Tag::Spectrum;
        (*tags)["rgb"]           = Tag::RGB;
        (*tags)["include"]       = Tag::Include;
        (*tags)["alias"]         = Tag::Alias;
        (*tags)["default"]       = Tag::Default;
        (*tags)["path"]          = Tag::Resource;
    }

    // Register the new class as an object tag
    const std::string &alias = class_->alias();

    if (tags->find(alias) == tags->end())
        (*tags)[alias] = Tag::Object;
    (*tag_class)[class_key(alias, class_->variant())] = class_;

    if (alias == "texture")
        (*tag_class)[class_key("spectrum", class_->variant())] = class_;
}

// Called by Class::static_shutdown()
void cleanup() {
    delete tags;
    delete tag_class;
    tags = nullptr;
    tag_class = nullptr;
}

/// Helper function: map a position offset in bytes to a more readable line/column value
static std::string string_offset(const std::string &string, ptrdiff_t pos) {
    std::istringstream is(string);
    char buffer[1024];
    int line = 0, line_start = 0, offset = 0;
    while (is.good()) {
        is.read(buffer, sizeof(buffer));
        for (int i = 0; i < is.gcount(); ++i) {
            if (buffer[i] == '\n') {
                if (offset + i >= pos)
                    return tfm::format("line %i, col %i", line + 1, pos - line_start);
                ++line;
                line_start = offset + i;
            }
        }
        offset += (int) is.gcount();
    }
    return "byte offset " + std::to_string(pos);
}

/// Helper function: map a position offset in bytes to a more readable line/column value
static std::string file_offset(const fs::path &filename, ptrdiff_t pos) {
    std::fstream is(filename.native());
    char buffer[1024];
    int line = 0, line_start = 0, offset = 0;
    while (is.good()) {
        is.read(buffer, sizeof(buffer));
        for (int i = 0; i < is.gcount(); ++i) {
            if (buffer[i] == '\n') {
                if (offset + i >= pos)
                    return tfm::format("line %i, col %i", line + 1, pos - line_start);
                ++line;
                line_start = offset + i;
            }
        }
        offset += (int) is.gcount();
    }
    return "byte offset " + std::to_string(pos);
}

struct XMLSource {
    std::string id;
    const pugi::xml_document &doc;
    std::function<std::string(ptrdiff_t)> offset;
    size_t depth = 0;
    bool modified = false;

    template <typename... Args>
    [[noreturn]]
    void throw_error(const pugi::xml_node &n, const std::string &msg_, Args&&... args) {
        std::string msg = "Error while loading \"%s\" (at %s): " + msg_ + ".";
        Throw(msg.c_str(), id, offset(n.offset_debug()), args...);
    }
};

struct XMLObject {
    Properties props;
    const Class *class_ = nullptr;
    std::string src_id;
    std::string alias;
    std::function<std::string(ptrdiff_t)> offset;
    size_t location = 0;
    ref<Object> object;
    std::mutex mutex;
};

enum class ColorMode {
    Monochromatic,
    RGB,
    Spectral
};

template <typename Float, typename Spectrum>
ColorMode variant_to_color_mode() {
    if constexpr (is_monochromatic_v<Spectrum>)
        return ColorMode::Monochromatic;
    else if constexpr (is_rgb_v<Spectrum>)
        return ColorMode::RGB;
    else if constexpr (is_spectral_v<Spectrum>)
        return ColorMode::Spectral;
    else
        static_assert(false_v<Float, Spectrum>, "This should never happen!");
}

struct XMLParseContext {
    std::string variant;
    bool parallel;

    std::unordered_map<std::string, XMLObject> instances;
    Transform4f transform;
    ColorMode color_mode;
    uint32_t id_counter = 0;
    uint32_t backend = 0;

    XMLParseContext(const std::string &variant, bool parallel)
        : variant(variant), parallel(parallel) {
        color_mode = MI_INVOKE_VARIANT(variant, variant_to_color_mode);

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
        if (string::starts_with(variant, "cuda_"))
            backend = (uint32_t) JitBackend::CUDA;
        else if (string::starts_with(variant, "llvm_"))
            backend = (uint32_t) JitBackend::LLVM;
#endif
    }
};

/// Helper function to check if attributes are fully specified
static void check_attributes(XMLSource &src, const pugi::xml_node &node,
                             std::set<std::string> &&attrs, bool expect_all = true) {
    bool found_one = false;
    for (auto attr : node.attributes()) {
        auto it = attrs.find(attr.name());
        if (it == attrs.end())
            src.throw_error(node, "unexpected attribute \"%s\" in element \"%s\"", attr.name(), node.name());
        attrs.erase(it);
        found_one = true;
    }
    if (!attrs.empty() && (!found_one || expect_all))
        src.throw_error(node, "missing attribute \"%s\" in element \"%s\"", *attrs.begin(), node.name());
}

/// Helper function to split the 'value' attribute into X/Y/Z components
void expand_value_to_xyz(XMLSource &src, pugi::xml_node &node) {
    if (node.attribute("value")) {
        auto list = string::tokenize(node.attribute("value").value());
        if (node.attribute("x") || node.attribute("y") || node.attribute("z"))
            src.throw_error(node, "can't mix and match \"value\" and \"x\"/\"y\"/\"z\" attributes");
        if (list.size() == 1) {
            node.append_attribute("x") = list[0].c_str();
            node.append_attribute("y") = list[0].c_str();
            node.append_attribute("z") = list[0].c_str();
        } else if (list.size() == 3) {
            node.append_attribute("x") = list[0].c_str();
            node.append_attribute("y") = list[1].c_str();
            node.append_attribute("z") = list[2].c_str();
        } else {
            src.throw_error(node, "\"value\" attribute must have exactly 1 or 3 elements");
        }
        node.remove_attribute("value");
    }
}

Vector3f parse_named_vector(XMLSource &src, pugi::xml_node &node, const std::string &attr_name) {
    auto vec_str = node.attribute(attr_name.c_str()).value();
    auto list = string::tokenize(vec_str);
    if (list.size() != 3)
        src.throw_error(node, "\"%s\" attribute must have exactly 3 elements", attr_name);
    try {
        return Vector3f(string::stof<Float>(list[0]),
                        string::stof<Float>(list[1]),
                        string::stof<Float>(list[2]));
    } catch (...) {
        src.throw_error(node, "could not parse floating point values in \"%s\"", vec_str);
    }
}

Vector3f parse_vector(XMLSource &src, pugi::xml_node &node, Float def_val = 0) {
    std::string value;
    try {
        Float x = def_val, y = def_val, z = def_val;
        value = node.attribute("x").value();
        if (!value.empty()) x = string::stof<Float>(value);
        value = node.attribute("y").value();
        if (!value.empty()) y = string::stof<Float>(value);
        value = node.attribute("z").value();
        if (!value.empty()) z = string::stof<Float>(value);
        return Vector3f(x, y, z);
    } catch (...) {
        src.throw_error(node, "could not parse floating point value \"%s\"", value);
    }
}

void upgrade_tree(XMLSource &src, pugi::xml_node &node, const Version &version) {
    if (version == Version(MI_VERSION_MAJOR, MI_VERSION_MINOR, MI_VERSION_PATCH))
        return;

    if (version < Version(2, 0, 0)) {
        // Upgrade all attribute names from camelCase to underscore_case
        for (pugi::xpath_node result: node.select_nodes("//*[@name]")) {
            pugi::xml_node n = result.node();
            if (strcmp(n.name(), "default") == 0)
                continue;
            pugi::xml_attribute name_attrib = n.attribute("name");
            std::string name = name_attrib.value();
            for (size_t i = 0; i < name.length() - 1; ++i) {
                if (std::islower(name[i]) && std::isupper(name[i + 1])) {
                    name = name.substr(0, i + 1) + std::string("_") + name.substr(i + 1);
                    i += 2;
                    while (i < name.length() && std::isupper(name[i])) {
                        name[i] = (char) std::tolower(name[i]);
                        ++i;
                    }
                }
            }
            name_attrib.set_value(name.c_str());
        }
        for (pugi::xpath_node result: node.select_nodes("//lookAt"))
            result.node().set_name("lookat");
        // automatically rename reserved identifiers
        for (pugi::xpath_node result: node.select_nodes("//@id")) {
            pugi::xml_attribute id_attrib = result.attribute();
            char const* val = id_attrib.value();
            if (val && val[0] == '_') {
                std::string new_id = std::string("ID") + val + "__UPGR";
                Log(Warn, "Changing identifier: \"%s\" -> \"%s\"", val, new_id.c_str());
                id_attrib = new_id.c_str();
            }
        }

        // changed parameters
        for (pugi::xpath_node result: node.select_nodes("//bsdf[@type='diffuse']/*/@name[.='diffuse_reflectance']"))
            result.attribute() = "reflectance";

        // Update 'uoffset', 'voffset', 'uscale', 'vscale' to transform block
        for (pugi::xpath_node result : node.select_nodes(
                 "//node()[float[@name='uoffset' or @name='voffset' or "
                 "@name='uscale' or @name='vscale']]")) {
            pugi::xml_node n = result.node();
            pugi::xml_node uoffset = n.select_node("float[@name='uoffset']").node();
            pugi::xml_node voffset = n.select_node("float[@name='voffset']").node();
            pugi::xml_node uscale  = n.select_node("float[@name='uscale']").node();
            pugi::xml_node vscale  = n.select_node("float[@name='vscale']").node();

            Vector2f offset(0.0), scale(1.0);
            if (uoffset) {
                offset.x() = string::stof<Float>(uoffset.attribute("value").value());
                n.remove_child(uoffset);
            }
            if (voffset) {
                offset.y() = string::stof<Float>(voffset.attribute("value").value());
                n.remove_child(voffset);
            }
            if (uscale) {
                scale.x() = string::stof<Float>(uscale.attribute("value").value());
                n.remove_child(uscale);
            }
            if (vscale) {
                scale.y() = string::stof<Float>(vscale.attribute("value").value());
                n.remove_child(vscale);
            }

            pugi::xml_node trafo = n.append_child("transform");
            trafo.append_attribute("name") = "to_uv";

            if (dr::all(offset != Vector2f(0.0))) {
                pugi::xml_node element = trafo.append_child("translate");
                element.append_attribute("x") = std::to_string(offset.x()).c_str();
                element.append_attribute("y") = std::to_string(offset.y()).c_str();
            }

            if (dr::all(scale != Vector2f(1.0))) {
                pugi::xml_node element = trafo.append_child("scale");
                element.append_attribute("x") = std::to_string(scale.x()).c_str();
                element.append_attribute("y") = std::to_string(scale.y()).c_str();
            }
        }

        src.modified |= true;
    }

    if (src.modified)
        Log(Info, "\"%s\": in-memory version upgrade (v%s -> v%s) ..", src.id, version,
            Version(MI_VERSION));
}

static std::pair<std::string, std::string> parse_xml(XMLSource &src, XMLParseContext &ctx,
                                                     pugi::xml_node &node, Tag parent_tag,
                                                     Properties &props, ParameterList &param,
                                                     size_t &arg_counter, int depth,
                                                     bool within_emitter = false,
                                                     bool within_spectrum = false) {
    try {
        if (!param.empty()) {
            std::sort(param.begin(), param.end(),
                [](const auto &a, const auto &b) -> bool {
                    return std::get<0>(a).length() > std::get<0>(b).length();
                });

            for (auto attr: node.attributes()) {
                std::string value = attr.value();
                if (value.find('$') == std::string::npos)
                    continue;
                for (auto &p : param) {
                    if (string::replace_inplace(value, "$" + std::get<0>(p), std::get<1>(p)))
                        std::get<2>(p) = true;
                }
                if (value.find('$') != std::string::npos)
                    src.throw_error(node, "undefined parameter(s) in string: \"%s\"!", value);
                attr.set_value(value.c_str());
            }
        }

        // Skip over comments
        if (node.type() == pugi::node_comment || node.type() == pugi::node_declaration)
            return std::make_pair("", "");

        if (node.type() != pugi::node_element)
            src.throw_error(node, "unexpected content");

        // Look up the name of the current element
        auto it = tags->find(node.name());
        if (it == tags->end())
            src.throw_error(node, "unexpected tag \"%s\"", node.name());

        Tag tag = it->second;

        if (node.attribute("type") && tag != Tag::Object
            && tag_class->find(class_key(node.name(), ctx.variant)) != tag_class->end())
            tag = Tag::Object;

        // Perform some safety checks to make sure that the XML tree really makes sense
        bool has_parent              = parent_tag != Tag::Invalid;
        bool parent_is_object        = has_parent && parent_tag == Tag::Object;
        bool current_is_object       = tag == Tag::Object;
        bool parent_is_transform     = parent_tag == Tag::Transform;
        bool current_is_transform_op = tag == Tag::Translate || tag == Tag::Rotate ||
                                       tag == Tag::Scale || tag == Tag::LookAt ||
                                       tag == Tag::Matrix;

        if (!has_parent && !current_is_object)
            src.throw_error(node, "root element \"%s\" must be an object", node.name());

        if (parent_is_transform != current_is_transform_op) {
            if (parent_is_transform)
                src.throw_error(node, "transform nodes can only contain transform operations");
            else
                src.throw_error(node, "transform operations can only occur in a transform node");
        }

        if (has_parent && !parent_is_object && !(parent_is_transform && current_is_transform_op))
            src.throw_error(node, "node \"%s\" cannot occur as child of a property", node.name());

        auto version_attr = node.attribute("version");

        if (depth == 0 && !version_attr)
            src.throw_error(node, "missing version attribute in root element \"%s\"", node.name());

        if (version_attr) {
            Version version;
            try {
                version = version_attr.value();
            } catch (const std::exception &) {
                src.throw_error(node, "could not parse version number \"%s\"", version_attr.value());
            }
            upgrade_tree(src, node, version);
            node.remove_attribute(version_attr);
        }

        if (std::string(node.name()) == "scene") {
            node.append_attribute("type") = "scene";
        } else if (tag == Tag::Transform) {
            ctx.transform = Transform4f();
        }

        if (node.attribute("name")) {
            auto name = node.attribute("name").value();
            if (string::starts_with(name, "_"))
                src.throw_error(
                    node, "invalid parameter name \"%s\" in element \"%s\": leading "
                          "underscores are reserved for internal identifiers.",
                    name, node.name());
        } else if (current_is_object || tag == Tag::NamedReference) {
            node.append_attribute("name") = tfm::format("_arg_%i", arg_counter++).c_str();
        }

        if (node.attribute("id")) {
            auto id = node.attribute("id").value();
            if (string::starts_with(id, "_"))
                src.throw_error(
                    node, "invalid id \"%s\" in element \"%s\": leading "
                          "underscores are reserved for internal identifiers.",
                    id, node.name());
        } else if (current_is_object) {
            node.append_attribute("id") = tfm::format("_unnamed_%u", ctx.id_counter++).c_str();
        }

        switch (tag) {
            case Tag::Object: {
                    check_attributes(src, node, { "type", "id", "name" });
                    std::string id        = node.attribute("id").value(),
                                name      = node.attribute("name").value(),
                                type      = node.attribute("type").value(),
                                node_name = node.name();

                    Properties props_nested(type);
                    props_nested.set_id(id);

                    auto it_inst = ctx.instances.find(id);
                    if (it_inst != ctx.instances.end())
                        src.throw_error(node, "\"%s\" has duplicate id \"%s\" (previous was at %s)",
                            node_name, id, src.offset(it_inst->second.location));

                    auto it2 = tag_class->find(class_key(node_name, ctx.variant));
                    if (it2 == tag_class->end())
                        src.throw_error(node, "could not retrieve class object for "
                                       "tag \"%s\" and variant \"%s\"", node_name, ctx.variant);

                    size_t arg_counter_nested = 0;
                    for (pugi::xml_node &ch: node.children()) {
                        auto [arg_name, nested_id] =
                            parse_xml(src, ctx, ch, tag, props_nested, param,
                                      arg_counter_nested, depth + 1,
                                      node_name == "emitter",
                                      node_name == "spectrum");
                        if (nested_id == id)
                            src.throw_error(node, "cannot reference parent id \"%s\" in nested object",
                                            nested_id);
                        if (!nested_id.empty())
                            props_nested.set_named_reference(arg_name, nested_id);
                    }

                    auto &inst = ctx.instances[id];
                    inst.props = props_nested;
                    inst.class_ = it2->second;
                    inst.offset = src.offset;
                    inst.src_id = src.id;
                    inst.location = node.offset_debug();
                    return std::make_pair(name, id);
                }
                break;

            case Tag::NamedReference: {
                    check_attributes(src, node, { "name", "id" });
                    auto id = node.attribute("id").value();
                    auto name = node.attribute("name").value();
                    return std::make_pair(name, id);
                }
                break;

            case Tag::Alias: {
                    check_attributes(src, node, { "id", "as" });
                    auto alias_src = node.attribute("id").value();
                    auto alias_dst = node.attribute("as").value();
                    auto it_alias_dst = ctx.instances.find(alias_dst);
                    if (it_alias_dst != ctx.instances.end())
                        src.throw_error(node, "\"%s\" has duplicate id \"%s\" (previous was at %s)",
                            node.name(), alias_dst, src.offset(it_alias_dst->second.location));
                    auto it_alias_src = ctx.instances.find(alias_src);
                    if (it_alias_src == ctx.instances.end())
                        src.throw_error(node, "referenced id \"%s\" not found", alias_src);

                    auto &inst = ctx.instances[alias_dst];
                    inst.alias = alias_src;
                    inst.offset = src.offset;
                    inst.src_id = src.id;
                    inst.location = node.offset_debug();

                    return std::make_pair("", "");
                }
                break;

            case Tag::Default: {
                    check_attributes(src, node, { "name", "value" });
                    std::string name = node.attribute("name").value();
                    std::string value = node.attribute("value").value();
                    if (name.empty())
                        src.throw_error(node, "<default>: name must by nonempty");

                    if (name.find(",") != std::string::npos)
                        src.throw_error(node, "Invalid character in parameter name: ',' in %s", name);

                    bool found = false;
                    for (auto &p: param) {
                        if (std::get<0>(p) == name)
                            found = true;
                    }
                    if (!found)
                        param.emplace_back(name, value, true);
                    return std::make_pair("", "");
                }
                break;

            case Tag::Resource: {
                    check_attributes(src, node, { "value" });
                    if (depth != 1)
                        src.throw_error(node, "<path>: path can only be child of root");
                    ref<FileResolver> fs = Thread::thread()->file_resolver();
                    fs::path resource_path(node.attribute("value").value());
                    if (!resource_path.is_absolute()) {
                        // First try to resolve it starting in the XML file directory
                        resource_path = fs::path(src.id).parent_path() / resource_path;
                        // Otherwise try to resolve it with the FileResolver
                        if (!fs::exists(resource_path))
                            resource_path = fs->resolve(node.attribute("value").value());
                    }
                    if (!fs::exists(resource_path))
                        src.throw_error(node, "<path>: folder \"%s\" not found", resource_path);
                    fs->prepend(resource_path);
                    return std::make_pair("", "");
                }
                break;

            case Tag::Include: {
                    check_attributes(src, node, { "filename" });
                    ref<FileResolver> fs = Thread::thread()->file_resolver();
                    fs::path filename = fs->resolve(node.attribute("filename").value());
                    if (!fs::exists(filename))
                        src.throw_error(node, "included file \"%s\" not found", filename);

                    Log(Info, "Loading included XML file \"%s\" ..", filename);

                    pugi::xml_document doc;
                    pugi::xml_parse_result result = doc.load_file(filename.native().c_str());

                    detail::XMLSource nested_src {
                        filename.string(), doc,
                        [=](ptrdiff_t pos) { return detail::file_offset(filename, pos); },
                        src.depth + 1
                    };

                    if (nested_src.depth > MI_XML_INCLUDE_MAX_RECURSION)
                        Throw("Exceeded <include> recursion limit of %i",
                              MI_XML_INCLUDE_MAX_RECURSION);

                    if (!result) // There was a parser / file IO error
                        src.throw_error(node, "error while loading \"%s\" (at %s): %s",
                            nested_src.id, nested_src.offset(result.offset),
                            result.description());

                    try {
                        if (std::string(doc.begin()->name()) == "scene") {
                            auto version_attr_incl = doc.begin()->attribute("version");
                            if (version_attr_incl) {
                                Version version;
                                try {
                                    version = version_attr_incl.value();
                                } catch (const std::exception &) {
                                    nested_src.throw_error(*doc.begin(), "could not parse version number \"%s\"", version_attr_incl.value());
                                }
                                upgrade_tree(nested_src, *doc.begin(), version);
                                doc.begin()->remove_attribute(version_attr_incl);
                            }

                            for (pugi::xml_node &ch: doc.begin()->children()) {
                                auto [arg_name, nested_id] = parse_xml(nested_src, ctx, ch, parent_tag,
                                          props, param, arg_counter, 1);
                                if (!nested_id.empty())
                                    props.set_named_reference(arg_name, nested_id);
                            }
                        } else {
                            return parse_xml(nested_src, ctx, *doc.begin(), parent_tag,
                                             props, param, arg_counter, 0);
                        }
                    } catch (const std::exception &e) {
                        src.throw_error(node, "%s", e.what());
                    }
                }
                break;

            case Tag::String: {
                    check_attributes(src, node, { "name", "value" });
                    props.set_string(node.attribute("name").value(), node.attribute("value").value());
                }
                break;

            case Tag::Float: {
                    check_attributes(src, node, { "name", "value" });
                    std::string value = node.attribute("value").value();
                    Float value_float;
                    try {
                        value_float = string::stof<Float>(value);
                    } catch (...) {
                        src.throw_error(node, "could not parse floating point value \"%s\"", value);
                    }
                    props.set_float(node.attribute("name").value(), value_float);
                }
                break;

            case Tag::Integer: {
                    check_attributes(src, node, { "name", "value" });
                    std::string value = node.attribute("value").value();
                    int64_t value_long;
                    try {
                        value_long = detail::stoll(value);
                    } catch (...) {
                        src.throw_error(node, "could not parse integer value \"%s\"", value);
                    }
                    props.set_long(node.attribute("name").value(), value_long);
                }
                break;

            case Tag::Boolean: {
                    check_attributes(src, node, { "name", "value" });
                    std::string value = string::to_lower(node.attribute("value").value());
                    bool result = false;
                    if (value == "true")
                        result = true;
                    else if (value == "false")
                        result = false;
                    else
                        src.throw_error(node, "could not parse boolean value "
                                             "\"%s\" -- must be \"true\" or "
                                             "\"false\"", value);
                    props.set_bool(node.attribute("name").value(), result);
                }
                break;

            case Tag::Vector: {
                    detail::expand_value_to_xyz(src, node);
                    check_attributes(src, node, { "name", "x", "y", "z" });
                    props.set_array3f(node.attribute("name").value(),
                                       detail::parse_vector(src, node));
                }
                break;

            case Tag::Point: {
                    detail::expand_value_to_xyz(src, node);
                    check_attributes(src, node, { "name", "x", "y", "z" });
                    props.set_array3f(node.attribute("name").value(),
                                      detail::parse_vector(src, node));
                }
                break;

            case Tag::RGB : {
                    check_attributes(src, node, { "name", "value" });
                    std::vector<std::string> tokens = string::tokenize(node.attribute("value").value());

                    if (tokens.size() == 1) {
                        tokens.push_back(tokens[0]);
                        tokens.push_back(tokens[0]);
                    }
                    if (tokens.size() != 3)
                        src.throw_error(node, "'rgb' tag requires one or three values (got \"%s\")",
                                        node.attribute("value").value());

                    Color3f color;
                    try {
                        color = Color3f(string::stof<Float>(tokens[0]),
                                        string::stof<Float>(tokens[1]),
                                        string::stof<Float>(tokens[2]));
                    } catch (...) {
                        src.throw_error(node, "could not parse RGB value \"%s\"", node.attribute("value").value());
                    }

                    if (!within_spectrum) {
                        std::string name = node.attribute("name").value();
                        ref<Object> obj = detail::create_texture_from_rgb(
                            name, color, ctx.variant, within_emitter);
                        props.set_object(name, obj);
                    } else {
                        props.set_color("color", color);
                    }
                }
                break;

            case Tag::Spectrum: {
                    check_attributes(src, node, { "name", "value", "filename" }, false);
                    std::string name = node.attribute("name").value();

                    ScalarFloat const_value(1);
                    std::vector<Float> wavelengths, values;

                    bool has_value = !node.attribute("value").empty(),
                         has_filename = !node.attribute("filename").empty(),
                         is_constant = has_value && string::tokenize(node.attribute("value").value()).size() == 1;
                    if (has_value == has_filename) {
                        src.throw_error(node, "'spectrum' tag requires one of \"value\" or \"filename\" attributes");
                    } else if (is_constant) {
                        /* A constant spectrum is specified. */
                        std::vector<std::string> tokens = string::tokenize(node.attribute("value").value());

                        try {
                            const_value = string::stof<Float>(tokens[0]);
                        } catch (...) {
                            src.throw_error(node, "could not parse constant spectrum \"%s\"", tokens[0]);
                        }
                    } else {
                        /* Parse wavelength:value pairs, either inlined or from an external file.
                           Wavelengths are expected to be specified in increasing order. */
                        if (has_value) {
                            std::vector<std::string> tokens = string::tokenize(node.attribute("value").value());

                            for (const std::string &token : tokens) {
                                std::vector<std::string> pair = string::tokenize(token, ":");
                                if (pair.size() != 2)
                                    src.throw_error(node, "invalid spectrum (expected wavelength:value pairs)");

                                Float wavelength, value;
                                try {
                                    wavelength = string::stof<Float>(pair[0]);
                                    value = string::stof<Float>(pair[1]);
                                } catch (...) {
                                    src.throw_error(node, "could not parse wavelength:value pair: \"%s\"", token);
                                }

                                wavelengths.push_back(wavelength);
                                values.push_back(value);
                            }
                        } else if (has_filename) {
                            spectrum_from_file(node.attribute("filename").value(), wavelengths, values);
                        }
                    }

                    ref<Object> obj = detail::create_texture_from_spectrum(
                        name, const_value, wavelengths, values, ctx.variant,
                        within_emitter,
                        ctx.color_mode == ColorMode::Spectral,
                        ctx.color_mode == ColorMode::Monochromatic);

                    props.set_object(name, obj);
                }
                break;

            case Tag::Transform: {
                    check_attributes(src, node, { "name" });
                    ctx.transform = Transform4f();
                }
                break;

            case Tag::Rotate: {
                    detail::expand_value_to_xyz(src, node);
                    check_attributes(src, node, { "angle", "x", "y", "z" }, false);
                    Vector3f vec = detail::parse_vector(src, node);
                    std::string angle = node.attribute("angle").value();
                    Float angle_float;
                    try {
                        angle_float = string::stof<Float>(angle);
                    } catch (...) {
                        src.throw_error(node, "could not parse floating point value \"%s\"", angle);
                    }
                    ctx.transform = Transform4f::rotate(vec, angle_float) * ctx.transform;
                }
                break;

            case Tag::Translate: {
                    detail::expand_value_to_xyz(src, node);
                    check_attributes(src, node, { "x", "y", "z" }, false);
                    Vector3f vec = detail::parse_vector(src, node);
                    ctx.transform = Transform4f::translate(vec) * ctx.transform;
                }
                break;

            case Tag::Scale: {
                    detail::expand_value_to_xyz(src, node);
                    check_attributes(src, node, { "x", "y", "z" }, false);
                    Vector3f vec = detail::parse_vector(src, node, 1);
                    ctx.transform = Transform4f::scale(vec) * ctx.transform;
                }
                break;

            case Tag::LookAt: {
                    if (!node.attribute("up"))
                        node.append_attribute("up") = "0,0,0";

                    check_attributes(src, node, { "origin", "target", "up" });

                    Point3f origin = parse_named_vector(src, node, "origin");
                    Point3f target = parse_named_vector(src, node, "target");
                    Vector3f up = parse_named_vector(src, node, "up");

                    if (dr::squared_norm(up) == 0)
                        std::tie(up, std::ignore) =
                            coordinate_system(dr::normalize(target - origin));

                    auto result = Transform4f::look_at(origin, target, up);
                    if (dr::any_nested(dr::isnan(result.matrix)))
                        src.throw_error(node, "invalid lookat transformation");
                    ctx.transform = result * ctx.transform;
                }
                break;

            case Tag::Matrix: {
                    check_attributes(src, node, { "value" });
                    std::vector<std::string> tokens = string::tokenize(node.attribute("value").value());
                    if (tokens.size() != 16 && tokens.size() != 9)
                        Throw("matrix: expected 16 or 9 values");
                    Matrix4f matrix;
                    if (tokens.size() == 16) {
                        for (int i = 0; i < 4; ++i) {
                            for (int j = 0; j < 4; ++j) {
                                try {
                                    matrix(i, j) = string::stof<Float>(tokens[i * 4 + j]);
                                } catch (...) {
                                    src.throw_error(node, "could not parse floating point value \"%s\"",
                                                    tokens[i * 4 + j]);
                                }
                            }
                        }
                    } else {
                        Log(Warn, "3x3 matrix will be stored as a 4x4 matrix, with the same last row and column as the identity matrix.");
                        Matrix3f mat3;
                        for (int i = 0; i < 3; ++i) {
                            for (int j = 0; j < 3; ++j) {
                                try {
                                    mat3(i, j) = string::stof<Float>(tokens[i * 3 + j]);
                                } catch (...) {
                                    src.throw_error(node, "could not parse floating point value \"%s\"",
                                                    tokens[i * 3 + j]);
                                }
                            }
                        }
                        matrix = Matrix4f(mat3);
                    }
                    ctx.transform = Transform4f(matrix) * ctx.transform;
                }
                break;

            default: Throw("Unhandled element \"%s\"", node.name());
        }

        for (pugi::xml_node &ch: node.children())
            parse_xml(src, ctx, ch, tag, props, param, arg_counter, depth + 1);

        if (tag == Tag::Transform)
            props.set_transform(node.attribute("name").value(), ctx.transform);
    } catch (const std::exception &e) {
        if (strstr(e.what(), "Error while loading") == nullptr)
            src.throw_error(node, "%s", e.what());
        else
            throw;
    }

    return std::make_pair("", "");
}

static std::string init_xml_parse_context_from_file(XMLParseContext &ctx,
                                                    const fs::path &filename_,
                                                    ParameterList param,
                                                    bool write_update) {
    fs::path filename = filename_;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.native().c_str(),
        pugi::parse_default |
        pugi::parse_comments);

    detail::XMLSource src{
        filename.string(), doc,
        [=](ptrdiff_t pos) { return detail::file_offset(filename, pos); }
    };

    if (!result) // There was a parser / file IO error
        Throw("Error while loading \"%s\" (at %s): %s", src.id,
            src.offset(result.offset), result.description());

    pugi::xml_node root = doc.document_element();
    Properties props;
    size_t arg_counter = 0; // Unused
    auto scene_id = parse_xml(src, ctx, root, Tag::Invalid, props,
                              param, arg_counter, 0).second;

    for (const auto& p : param) {
        if (!std::get<2>(p))
            Throw("Unused parameter \"%s\"!", std::get<0>(p));
    }

    if (src.modified && write_update) {
        fs::path backup = filename;
        backup.replace_extension(".bak");
        Log(Info, "Writing updated \"%s\" .. (backup at \"%s\")", filename, backup);
        if (!fs::rename(filename, backup))
            Throw("Unable to rename file \"%s\" to \"%s\"!", filename, backup);

        // Update version number
        root.prepend_attribute("version").set_value(MI_VERSION);
        if (root.attribute("type").value() == std::string("scene"))
            root.remove_attribute("type");

        // Strip anonymous IDs/names
        for (pugi::xpath_node result2 : doc.select_nodes("//*[starts-with(@id, '_unnamed_')]"))
            result2.node().remove_attribute("id");
        for (pugi::xpath_node result2 : doc.select_nodes("//*[starts-with(@name, '_arg_')]"))
            result2.node().remove_attribute("name");

        doc.save_file(filename.native().c_str(), "    ");

        // Update for detail::file_offset
        filename = backup;
    }

    return scene_id;
}

static Task *instantiate_node(XMLParseContext &ctx,
                              const std::string &id,
                              ThreadEnvironment &env,
                              std::unordered_map<std::string, Task *> &task_map,
                              bool top_node) {
    if (task_map.find(id) != task_map.end())
        return task_map.find(id)->second;

    auto it = ctx.instances.find(id);
    if (it == ctx.instances.end())
        Throw("reference to unknown object \"%s\"!", id);

    XMLObject &inst = it->second;

    if (!inst.alias.empty())
        return instantiate_node(ctx, inst.alias, env, task_map, top_node);

    Properties &props = inst.props;
    const auto &named_references = props.named_references();
    // Recursive graph traversal to gather dependency tasks
    std::vector<Task *> deps;
    for (auto &kv : named_references) {
        const std::string& child_id = kv.second;
        if (task_map.find(child_id) == task_map.end()) {
            Task *task = instantiate_node(ctx, child_id, env, task_map, false);
            task_map.insert({child_id, task});
        }
        deps.push_back(task_map.find(child_id)->second);
    }

    uint32_t scope = 0;
#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
    // Deterministically assign a scope to each scene object
    if (ctx.backend && ctx.parallel) {
        jit_new_scope((JitBackend) ctx.backend);
        scope = jit_scope((JitBackend) ctx.backend);
    }
#endif

    auto instantiate = [&ctx, &env, id, scope]() {
        ScopedSetThreadEnvironment set_env(env);
        ScopedSetJITScope set_scope(ctx.parallel ? ctx.backend : 0u, scope);

        auto it = ctx.instances.find(id);
        if (it == ctx.instances.end())
            Throw("reference to unknown object \"%s\"!", id);

        auto &inst = it->second;
        Properties &props = inst.props;
        const auto &named_references = props.named_references();

        // Populate props with the already instantiated child objects
        for (auto &kv : named_references) {
            const std::string& child_id = kv.second;
            auto it2 = ctx.instances.find(child_id);
            if (it2 == ctx.instances.end())
                Throw("reference to unknown object \"%s\"!", child_id);
            ref<Object> obj = it2->second.object;
            Assert(obj);

            // Give the object a chance to recursively expand into sub-objects
            std::vector<ref<Object>> children = obj->expand();
            if (children.empty()) {
                props.set_object(kv.first, obj, false);
            } else if (children.size() == 1) {
                props.set_object(kv.first, children[0], false);
            } else {
                int ctr = 0;
                for (auto c : children)
                    props.set_object(kv.first + "_" + std::to_string(ctr++), c, false);
            }
        }

        try {
            inst.object = PluginManager::instance()->create_object(props, inst.class_);
        } catch (const std::exception &e) {
            Throw("Error while loading \"%s\" (near %s): could not instantiate "
                  "%s plugin of type \"%s\": %s",
                  inst.src_id, inst.offset(inst.location),
                  string::to_lower(inst.class_->name()), props.plugin_name(),
                  e.what());
        }

        auto unqueried = props.unqueried();
        if (!unqueried.empty()) {
            for (auto &v : unqueried) {
                if (props.type(v) == Properties::Type::Object) {
                    const auto &obj = props.object(v);
                    Throw("Error while loading \"%s\" (near %s): unreferenced "
                          "object %s (within %s of type \"%s\")",
                          inst.src_id, inst.offset(inst.location),
                          obj, string::to_lower(inst.class_->name()),
                          inst.props.plugin_name());
                } else {
                    v = "\"" + v + "\"";
                }
            }
            Throw("Error while loading \"%s\" (near %s): unreferenced %s "
                  "\"%s\" in %s plugin of type \"%s\"",
                  inst.src_id, inst.offset(inst.location),
                  unqueried.size() > 1 ? "properties" : "property", unqueried,
                  string::to_lower(inst.class_->name()), props.plugin_name());
        }
    };

    if (top_node) {
        // Top node always instantiated on the main thread
        std::exception_ptr eptr;
        for (auto& task : deps) {
            try {
                task_wait(task);
            } catch (...) {
                if (!eptr)
                    eptr = std::current_exception();
            }
        }
        for (auto& kv : task_map)
            task_release(kv.second);
        if (eptr)
            std::rethrow_exception(eptr);
        instantiate();
        return nullptr;
    } else {
        if (ctx.parallel) {
            // Instantiate object asynchronously
            return dr::do_async(instantiate, deps.data(), deps.size());
        } else {
            instantiate();
            return nullptr;
        }
    }
}

static ref<Object> instantiate_top_node(XMLParseContext &ctx, const std::string &id) {
    ThreadEnvironment env;
    std::unordered_map<std::string, Task*> task_map;
    instantiate_node(ctx, id, env, task_map, true);
#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
    if (ctx.backend && ctx.parallel)
        jit_new_scope((JitBackend) ctx.backend);
#endif
    return ctx.instances.find(id)->second.object;
}

ref<Object> create_texture_from_rgb(const std::string &name,
                                    Color<float, 3> color,
                                    const std::string &variant,
                                    bool within_emitter) {
    Properties props(within_emitter ? "d65" : "srgb");
    props.set_color("color", color);

    if (!within_emitter && is_unbounded_spectrum(name))
        props.set_bool("unbounded", true);

    ref<Object> texture = PluginManager::instance()->create_object(
        props, Class::for_name("Texture", variant));
    std::vector<ref<Object>> children = texture->expand();
    if (!children.empty())
        return (Object *) children[0].get();
    return texture;
}

ref<Object> create_texture_from_spectrum(const std::string &name,
                                         Float const_value,
                                         std::vector<Float> &wavelengths,
                                         std::vector<Float> &values,
                                         const std::string &variant,
                                         bool within_emitter,
                                         bool is_spectral_mode,
                                         bool is_monochromatic_mode) {
    const Class *class_ = Class::for_name("Texture", variant);

    bool is_unbounded = is_unbounded_spectrum(name);
    if (wavelengths.empty()) {
        if (!is_spectral_mode && within_emitter && !is_unbounded) {
            /* A uniform spectrum does not produce a uniform RGB response in the
               sRGB (which has a D65 white point). The call to 'xyz_to_srgb'
               computes this purple-ish color and uses it to initialize the
               'srgb' plugin. This is needed mainly for consistency between RGB
               and spectral variants of Mitsuba. */
            Color3f color = const_value * xyz_to_srgb(Color3f(1.0));
            Properties props("srgb");
            props.set_color("color", color);
            props.set_bool("unbounded", true);
            return PluginManager::instance()->create_object(props, class_);
        } else {
            Properties props("uniform");
            props.set_float("value", const_value);
            return PluginManager::instance()->create_object(props, class_);
        }
    } else {
        /* Detect whether wavelengths are regularly sampled and potentially
           apply the conversion factor. */
        Float min_interval = std::numeric_limits<Float>::infinity(),
              max_interval = 0.0;

        /* Values should be scaled so that integrating the spectrum against the
           CIE curves and converting to sRGB yields (1, 1, 1) for D65 in
           non-spectral modes. */
        Float unit_conversion =
            !is_spectral_mode ? Float(MI_CIE_Y_NORMALIZATION) : Float(1.f);

        for (size_t n = 0; n < wavelengths.size(); ++n) {
            values[n] *= unit_conversion;

            if (n <= 0)
                continue;

            Float distance = (wavelengths[n] - wavelengths[n - 1]);
            if (distance < 0)
                Throw("Wavelengths must be specified in increasing order!");

            min_interval = std::min(distance, min_interval);
            max_interval = std::max(distance, max_interval);
        }

        if (is_spectral_mode) {
            Properties props;

            /* The regular spectrum class is more efficient, so tolerate a small
               amount of imprecision in the parsed interval positions.. */
            bool is_regular =
                (max_interval - min_interval) < Float(1e-3) * min_interval;

            if (is_regular) {
                props.set_plugin_name("regular");
                props.set_long("size", wavelengths.size());
                props.set_float("wavelength_min", wavelengths.front());
                props.set_float("wavelength_max", wavelengths.back());
                props.set_pointer("values", values.data());
            } else {
                props.set_plugin_name("irregular");
                props.set_long("size", wavelengths.size());
                props.set_pointer("wavelengths", wavelengths.data());
                props.set_pointer("values", values.data());
            }

            return PluginManager::instance()->create_object(props, class_);
        } else {
            /* Pre-integrate against the CIE matching curves. In order to match
               the behavior of spectral modes, this function should instead
               pre-integrate against the product of the CIE curves and the CIE
               D65 curve in the case of reflectance values. */
            Color3f color = spectrum_list_to_srgb(
                wavelengths, values,
                /* bounded */ !(within_emitter || is_unbounded),
                /* d65 */ !(within_emitter && !is_unbounded));


            Properties props;
            if (is_monochromatic_mode) {
                props = Properties("uniform");
                props.set_float("value", luminance(color));
            } else {
                props = Properties("srgb");
                props.set_color("color", color);

                if (within_emitter || is_unbounded)
                    props.set_bool("unbounded", true);
            }

            return PluginManager::instance()->create_object(props, class_);
        }
    }
}

std::vector<ref<Object>> expand_node(const ref<Object> &node) {
    std::vector<ref<Object>> sub_objects = node->expand();
    if (!sub_objects.empty())
        return sub_objects;

    return { node };
}

std::vector<std::pair<std::string, Properties>> xml_to_properties(const fs::path &filename,
                                                                  const std::string &variant) {
    if (!fs::exists(filename))
        Throw("\"%s\": file does not exist!", filename);

    Timer timer;
    Log(Info, "Loading XML file \"%s\" with variant \"%s\"..", filename,
        variant);

    // Make a backup copy of the FileResolver, which will be restored after
    // parsing
    ref<FileResolver> fs_backup = Thread::thread()->file_resolver();
    ref<FileResolver> fs        = new FileResolver(*fs_backup);
    fs->append(filename.parent_path());
    Thread::thread()->set_file_resolver(fs.get());

    try {
        ParameterList param;
        detail::XMLParseContext ctx(variant, false);
        (void) detail::init_xml_parse_context_from_file(ctx, filename, param, false);

        Thread::thread()->set_file_resolver(fs_backup.get());

        Log(Info, "Done loading XML file \"%s\" (took %s).", filename,
            util::time_string((float) timer.value(), true));

        // Copy all properties inside of a vector. The object hierarchy is handled
        // using named reference properties.
        std::vector<std::pair<std::string, Properties>> props;
        for (auto &[id, object] : ctx.instances) {
            if (!object.class_) {
                Log(Warn, "Cannot find class for property with id \"%s\".", id);
                continue;
            }
            props.emplace_back(object.class_->name(), std::move(object.props));
        }

        return props;
    } catch (...) {
        Thread::thread()->set_file_resolver(fs_backup.get());
        throw;
    }
}

NAMESPACE_END(detail)

std::vector<ref<Object>> load_string(const std::string &string,
                                     const std::string &variant,
                                     ParameterList param,
                                     bool parallel) {
    ScopedPhase sp(ProfilerPhase::InitScene);
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(string.c_str(), string.length(),
                                                    pugi::parse_default |
                                                    pugi::parse_comments);
    detail::XMLSource src{
        "<string>", doc,
        [&](ptrdiff_t pos) { return detail::string_offset(string, pos); }
    };

    if (!result) // There was a parser error
        Throw("Error while loading \"%s\" (at %s): %s", src.id,
              src.offset(result.offset), result.description());

    // Make a backup copy of the FileResolver, which will be restored after parsing
    ref<FileResolver> fs_backup = Thread::thread()->file_resolver();
    Thread::thread()->set_file_resolver(new FileResolver(*fs_backup));

    try {
        pugi::xml_node root = doc.document_element();
        detail::XMLParseContext ctx(variant, parallel);
        Properties props;
        size_t arg_counter = 0; // Unused
        auto scene_id = detail::parse_xml(src, ctx, root, Tag::Invalid, props,
                                          param, arg_counter, 0).second;

        for (const auto& p : param) {
            if (!std::get<2>(p))
                Throw("Unused parameter \"%s\"!", std::get<0>(p));
        }

        ref<Object> top_node = detail::instantiate_top_node(ctx, scene_id);
        std::vector<ref<Object>> objects = detail::expand_node(top_node);

        Thread::thread()->set_file_resolver(fs_backup.get());
        return objects;
    } catch(...) {
        Thread::thread()->set_file_resolver(fs_backup.get());
        throw;
    }
}

std::vector<ref<Object>> load_file(const fs::path &filename,
                                   const std::string &variant,
                                   ParameterList param,
                                   bool write_update,
                                   bool parallel) {
    ScopedPhase sp(ProfilerPhase::InitScene);

    if (!fs::exists(filename))
        Throw("\"%s\": file does not exist!", filename);

    Timer timer;
    Log(Info, "Loading XML file \"%s\" with variant \"%s\"..", filename, variant);

    // Make a backup copy of the FileResolver, which will be restored after parsing
    ref<FileResolver> fs_backup = Thread::thread()->file_resolver();
    ref<FileResolver> fs = new FileResolver(*fs_backup);
    fs->append(filename.parent_path());
    Thread::thread()->set_file_resolver(fs.get());

    try {
        detail::XMLParseContext ctx(variant, parallel);
        auto scene_id = detail::init_xml_parse_context_from_file(ctx, filename, param, write_update);

        ref<Object> top_node = detail::instantiate_top_node(ctx, scene_id);
        std::vector<ref<Object>> objects = detail::expand_node(top_node);

        Thread::thread()->set_file_resolver(fs_backup.get());

        Log(Info, "Done loading XML file \"%s\" (took %s).",
            filename, util::time_string((float) timer.value(), true));

        return objects;
    } catch (...) {
        Thread::thread()->set_file_resolver(fs_backup.get());
        throw;
    }
}

NAMESPACE_END(xml)
NAMESPACE_END(mitsuba)
