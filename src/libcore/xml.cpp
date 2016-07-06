#include <mitsuba/core/xml.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/class.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/transform.h>
#include <pugixml.hpp>
#include <tbb/tbb.h>

#include <fstream>
#include <set>
#include <unordered_map>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)

/* Set of supported XML tags */
enum ETag {
    EBoolean,
    EInteger,
    EFloat,
    EString,
    EPoint,
    EVector,
    EColor,
    ETransform,
    ETranslate,
    EMatrix,
    ERotate,
    EScale,
    ELookAt,
    EObject,
    ENamedReference,
    EInvalid
};

NAMESPACE_BEGIN(detail)

static std::unordered_map<std::string, ETag> *tags = nullptr;
static std::unordered_map<std::string, const Class *> *tagClass = nullptr;

// Called by Class::Class()
void registerClass(const Class *class_) {
    if (!tags) {
        tags = new std::unordered_map<std::string, ETag>();
        tagClass = new std::unordered_map<std::string, const Class *>();

        /* Create an initial mapping of tag names to IDs */
        (*tags)["boolean"]    = EBoolean;
        (*tags)["integer"]    = EInteger;
        (*tags)["float"]      = EFloat;
        (*tags)["string"]     = EString;
        (*tags)["point"]      = EPoint;
        (*tags)["vector"]     = EVector;
        (*tags)["color"]      = EColor;
        (*tags)["transform"]  = ETransform;
        (*tags)["translate"]  = ETranslate;
        (*tags)["matrix"]     = EMatrix;
        (*tags)["rotate"]     = ERotate;
        (*tags)["scale"]      = EScale;
        (*tags)["lookat"]     = ELookAt;
        (*tags)["ref"]        = ENamedReference;
    }

    /* Register the new class as an object tag */
    auto tagName = string::toLower(class_->name());
    if (tags->find(tagName) == tags->end()) {
        (*tags)[tagName] = EObject;
        (*tagClass)[tagName] = class_;
    }
}

// Called by Class::staticShutdown()
void cleanup() {
    delete tags;
    delete tagClass;
}

/// Helper function: map a position offset in bytes to a more readable line/column value
static std::string stringOffset(const std::string &string, ptrdiff_t pos) {
    std::istringstream is(string);
    char buffer[1024];
    int line = 0, linestart = 0, offset = 0;
    while (is.good()) {
        is.read(buffer, sizeof(buffer));
        for (int i = 0; i < is.gcount(); ++i) {
            if (buffer[i] == '\n') {
                if (offset + i >= pos)
                    return tfm::format("line %i, col %i", line + 1, pos - linestart);
                ++line;
                linestart = offset + i;
            }
        }
        offset += (int) is.gcount();
    }
    return "byte offset " + std::to_string(pos);
}

/// Helper function: map a position offset in bytes to a more readable line/column value
static std::string fileOffset(const fs::path &filename, ptrdiff_t pos) {
    std::fstream is(filename.native());
    char buffer[1024];
    int line = 0, linestart = 0, offset = 0;
    while (is.good()) {
        is.read(buffer, sizeof(buffer));
        for (int i = 0; i < is.gcount(); ++i) {
            if (buffer[i] == '\n') {
                if (offset + i >= pos)
                    return tfm::format("line %i, col %i", line + 1, pos - linestart);
                ++line;
                linestart = offset + i;
            }
        }
        offset += (int) is.gcount();
    }
    return "byte offset " + std::to_string(pos);
}

struct XMLSource {
    std::string id;
    const pugi::xml_document &doc;
    const std::function<std::string(ptrdiff_t)> &offset;

    template <typename... Args>
    void throwError(const pugi::xml_node &n, const std::string &msg_, Args&&... args) {
        std::string msg = "Error while parsing \"%s\" (at %s): " + msg_;
        Throw(msg.c_str(), id, offset(n.offset_debug()), args...);
    }
};

struct XMLObject {
    Properties props;
    ref<Object> object;
    const Class *class_ = nullptr;
    size_t location = 0;
    tbb::spin_mutex mutex;
};

struct XMLParseContext {
    std::unordered_map<std::string, XMLObject> instances;
    Eigen::Affine3f transform = Eigen::Affine3f::Identity();
    size_t idCounter = 0;
};


/// Helper function to check if attributes are fully specified
static void checkAttributes(XMLSource &src, const pugi::xml_node &node, std::set<std::string> &&attrs) {
    for (auto attr : node.attributes()) {
        auto it = attrs.find(attr.name());
        if (it == attrs.end())
            src.throwError(node, "unexpected attribute \"%s\" in \"%s\"", attr.name(), node.name());
        attrs.erase(it);
    }
    if (!attrs.empty())
        src.throwError(node, "missing attribute \"%s\" in \"%s\"", *attrs.begin(), node.name());
}

static std::pair<std::string, std::string>
parseXML(XMLSource &src, XMLParseContext &ctx, pugi::xml_node &node,
         ETag parentTag, Properties &props, size_t &argCounter) {
    try {
        /* Skip over comments */
        if (node.type() == pugi::node_comment || node.type() == pugi::node_declaration)
            return std::make_pair("", "");

        if (node.type() != pugi::node_element)
            src.throwError(node, "unexpected content");

        /* Look up the name of the current element */
        auto it = tags->find(node.name());
        if (it == tags->end())
            src.throwError(node, "unexpected tag \"%s\"", node.name());

        ETag tag = it->second;

        /* Perform some safety checks to make sure that the XML tree really makes sense */
        bool hasParent            = parentTag != EInvalid;
        bool parentIsObject       = hasParent && parentTag == EObject;
        bool currentIsObject      = tag == EObject;
        bool parentIsTransform    = parentTag == ETransform;
        bool currentIsTransformOp = tag == ETranslate || tag == ERotate ||
                                    tag == EScale || tag == ELookAt ||
                                    tag == EMatrix;

        if (!hasParent && !currentIsObject)
            src.throwError(node, "root element \"%s\" must be an object", node.name());

        if (parentIsTransform != currentIsTransformOp) {
            if (parentIsTransform)
                src.throwError(node, "transform nodes can only contain transform operations");
            else
                src.throwError(node, "transform operations can only occur in a transform node");
        }

        if (hasParent && !parentIsObject && !(parentIsTransform && currentIsTransformOp))
            src.throwError(node, "node \"%s\" cannot occur as child of a property", node.name());

        if (std::string(node.name()) == "scene") {
            checkAttributes(src, node, { "version" });
            node.append_attribute("type") = "scene";
            node.remove_attribute("version");
        } else if (tag == ETransform) {
            ctx.transform.setIdentity();
        }

        if (node.attribute("name")) {
            auto name = node.attribute("name").value();
            if (string::startsWith(name, "_"))
                src.throwError(
                    node, "invalid parameter name \"%s\" in \"%s\": leading "
                          "underscores are reserved for internal identifiers",
                    name, node.name());
        } else if (currentIsObject || tag == ENamedReference) {
            node.append_attribute("name") = tfm::format("_arg_%i", argCounter++).c_str();
        }

        if (node.attribute("id")) {
            auto id = node.attribute("id").value();
            if (string::startsWith(id, "_"))
                src.throwError(
                    node, "invalid id \"%s\" in \"%s\": leading "
                          "underscores are reserved for internal identifiers",
                    id, node.name());
        } else if (currentIsObject) {
            node.append_attribute("id") = tfm::format("_unnamed_%i", ctx.idCounter++).c_str();
        }

        switch (tag) {
            case EObject: {
                    checkAttributes(src, node, { "type", "id", "name" });
                    auto id = node.attribute("id").value();
                    auto name = node.attribute("name").value();

                    Properties propsNested(node.attribute("type").value());
                    propsNested.setID(id);

                    auto it = ctx.instances.find(id);
                    if (it != ctx.instances.end())
                        src.throwError(node, "\"%s\" has duplicate id \"%s\" (previous was at %s)",
                            node.name(), id, src.offset(it->second.location));

                    auto it2 = tagClass->find(node.name());
                    if (it2 == tagClass->end())
                        src.throwError(node, "could not retrieve class object for "
                                       "tag \"%s\"", node.name());

                    size_t argCounterNested = 0;
                    for (pugi::xml_node &ch: node.children()) {
                        std::string nestedID, argName;
                        std::tie(argName, nestedID) = parseXML(src, ctx, ch, tag, propsNested, argCounterNested);
                        if (!nestedID.empty())
                            propsNested.setNamedReference(argName, nestedID);
                    }

                    auto &inst = ctx.instances[id];
                    inst.location = node.offset_debug();
                    inst.props = propsNested;
                    inst.class_ = it2->second;
                    return std::make_pair(name, id);
                }
                break;

            case ENamedReference: {
                    checkAttributes(src, node, { "name", "id" });
                    auto id = node.attribute("id").value();
                    auto name = node.attribute("name").value();
                    return std::make_pair(name, id);
                }
                break;

            case EString: {
                    checkAttributes(src, node, { "name", "value" });
                    props.setString(node.attribute("name").value(), node.attribute("value").value());
                }
                break;

            case EFloat: {
                    checkAttributes(src, node, { "name", "value" });
                    props.setFloat(node.attribute("name").value(), std::stof(node.attribute("value").value()));
                }
                break;

            case EInteger: {
                    checkAttributes(src, node, { "name", "value" });
                    props.setInt(node.attribute("name").value(), std::stoi(node.attribute("value").value()));
                }
                break;

            case EBoolean: {
                    checkAttributes(src, node, { "name", "value" });
                    //props.setBool(node.attribute("name").value(), toBool(node.attribute("value").value()));
                }
                break;

            case EPoint: {
                    checkAttributes(src, node, { "name", "value" });
                    //props.setPoint(node.attribute("name").value(), Point3f(toVector3f(node.attribute("value").value())));
                }
                break;

            case EVector: {
                    checkAttributes(src, node, { "name", "value" });
                    //props.setVector(node.attribute("name").value(), Vector3f(toVector3f(node.attribute("value").value())));
                }
                break;

            case EColor: {
                    checkAttributes(src, node, { "name", "value" });
                    //props.setColor(node.attribute("name").value(), Color3f(toVector3f(node.attribute("value").value()).array()));
                }
                break;

            case ETransform: {
                    checkAttributes(src, node, { "name" });
                    //props.setTransform(node.attribute("name").value(), transform.matrix());
                }
                break;

            case ETranslate: {
                    checkAttributes(src, node, { "value" });
                    //Eigen::Vector3f v = toVector3f(node.attribute("value").value());
                    //transform = Eigen::Translation<float, 3>(v.x(), v.y(), v.z()) * transform;
                }
                break;

            case EMatrix: {
                    checkAttributes(src, node, { "value" });
                    std::vector<std::string> tokens = string::tokenize(node.attribute("value").value());
                    if (tokens.size() != 16)
                        Throw("matrix: expected 16 values");
                    Eigen::Matrix4f matrix;
                    for (int i=0; i<4; ++i)
                        for (int j=0; j<4; ++j)
                            matrix(i, j) = std::stof(tokens[i*4+j]);
                    ctx.transform = Eigen::Affine3f(matrix) * ctx.transform;
                }
                break;

            case EScale: {
                    checkAttributes(src, node, { "value" });
                    //Eigen::Vector3f v = toVector3f(node.attribute("value").value());
                    //transform = Eigen::DiagonalMatrix<float, 3>(v) * transform;
                }
                break;

            case ERotate: {
                    checkAttributes(src, node, { "angle", "axis" });
                    //float angle = math::degToRad(std::stof(node.attribute("angle").value()));
                    //Eigen::Vector3f axis = toVector3f(node.attribute("axis").value());
                    //transform = Eigen::AngleAxis<float>(angle, axis) * transform;
                }
                break;

            case ELookAt: {
                    checkAttributes(src, node, { "origin", "target", "up" });
                    //Eigen::Vector3f origin = toVector3f(node.attribute("origin").value());
                    //Eigen::Vector3f target = toVector3f(node.attribute("target").value());
                    //Eigen::Vector3f up = toVector3f(node.attribute("up").value());

                    //Vector3f dir = (target - origin).normalized();
                    //Vector3f left = up.normalized().cross(dir).normalized();
                    //Vector3f newUp = dir.cross(left).normalized();

                    //Eigen::Matrix4f trafo;
                    //trafo << left, newUp, dir, origin,
                              //0, 0, 0, 1;

                    //transform = Eigen::Affine3f(trafo) * transform;
                }
                break;

            default: Throw("Unhandled element \"%s\"", node.name());
        }

        for (pugi::xml_node &ch: node.children())
            parseXML(src, ctx, ch, tag, props, argCounter);
    } catch (const std::exception &e) {
        if (strstr(e.what(), "Error while parsing") == nullptr)
            src.throwError(node, "%s", e.what());
        else
            throw;
    }

    return std::make_pair("", "");
}

static ref<Object> instantiateNode(XMLSource &src, XMLParseContext &ctx, std::string id) {
    auto it = ctx.instances.find(id);
    if (it == ctx.instances.end())
        Throw("reference to unknown object \"%s\"!", id);

    auto &inst = it->second;
    tbb::spin_mutex::scoped_lock lock(inst.mutex);

    if (inst.object)
        return inst.object;

    Properties &props = inst.props;
    auto namedReferences = props.namedReferences();

    Thread *thread = Thread::thread();

    tbb::parallel_for(tbb::blocked_range<uint32_t>(
        0u, (uint32_t) namedReferences.size(), 1),
        [&](const tbb::blocked_range<uint32_t> &range) {
            ThreadEnvironment env(thread);
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                auto &kv = namedReferences[i];
                try {
                    ref<Object> obj = instantiateNode(src, ctx, kv.second);
                    props.setObject(kv.first, obj, false);
                } catch (const std::exception &e) {
                    if (strstr(e.what(), "Error while loading") == nullptr)
                        Throw("Error while loading \"%s\" (near %s): %s",
                            src.id, src.offset(inst.location), e.what());
                    else
                        throw;
                }
            }
        }
    );

    try {
        inst.object = PluginManager::instance()->createObject(inst.class_, props);
        return inst.object;
    } catch (const std::exception &e) {
        Throw("Error while loading \"%s\" (near %s): could not instantiate "
              "\"%s\" plugin \"%s\": %s", src.id, src.offset(inst.location),
              string::toLower(inst.class_->name()), props.pluginName(),
              e.what());
    }
}

NAMESPACE_END(detail)

ref<Object> loadString(const std::string &string) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(string.c_str(), string.length());

    detail::XMLSource src{
        "<string>", doc,
        [&](ptrdiff_t pos) { return detail::stringOffset(string, pos); }
    };

    if (!result) /* There was a parser / file IO error */
        Throw("Error while parsing \"%s\": %s (at %s)", src.id,
              result.description(), src.offset(result.offset));

    detail::XMLParseContext ctx;
    Properties prop; size_t argCounter; /* Unused */
    auto sceneID = detail::parseXML(src, ctx, *doc.begin(), EInvalid, prop, argCounter).second;
    return detail::instantiateNode(src, ctx, sceneID);
}

ref<Object> loadFile(const fs::path &filename) {
    if (!fs::exists(filename))
        Throw("\"%s\": file does not exist!", filename);

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.native().c_str());

    detail::XMLSource src {
        filename.string(), doc,
        [&](ptrdiff_t pos) { return detail::fileOffset(filename, pos); }
    };

    if (!result) /* There was a parser / file IO error */
        Throw("Error while parsing \"%s\": %s (at %s)", src.id,
              result.description(), src.offset(result.offset));

    detail::XMLParseContext ctx;
    Properties prop; size_t argCounter; /* Unused */
    auto sceneID = parseXML(src, ctx, *doc.begin(), EInvalid, prop, argCounter).second;
    return detail::instantiateNode(src, ctx, sceneID);
}

NAMESPACE_END(xml)
NAMESPACE_END(mitsuba)
