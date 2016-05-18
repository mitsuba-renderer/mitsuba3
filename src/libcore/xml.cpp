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

#include <pugixml.hpp>
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
    EInvalid,
    EObject
};

static std::unordered_map<std::string, ETag> *tags = nullptr;
static std::unordered_map<std::string, const Class *> *tagClass = nullptr;

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
    }

    /* Register the new class as an object tag */
    auto tagName = string::toLower(class_->getName());
    if (tags->find(tagName) == tags->end()) {
        (*tags)[tagName] = EObject;
        (*tagClass)[tagName] = class_;
    }
}

void cleanup() {
    delete tags;
    delete tagClass;
}

/* Forward declaration */
static ref<Object> loadImpl(const std::string &id,
                            const pugi::xml_parse_result &result,
                            const pugi::xml_document &doc,
                            const std::function<std::string(ptrdiff_t)> &offset);

ref<Object> loadString(const std::string &string) {
    /* Load the XML file using pugixml (a tiny self-contained XML parser implemented in C++) */
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(string.c_str(), string.length());

    /* Helper function: map a position offset in bytes to a more readable line/column value */
    auto offset = [&](ptrdiff_t pos) -> std::string {
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
    };

    return loadImpl("<string>", result, doc, offset);
}

ref<Object> loadFile(const fs::path &filename) {
    if (!fs::exists(filename))
        Throw("\"%s\": file does not exist!", filename);

    /* Load the XML file using pugixml (a tiny self-contained XML parser implemented in C++) */
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.native().c_str());

    /* Helper function: map a position offset in bytes to a more readable line/column value */
    auto offset = [&](ptrdiff_t pos) -> std::string {
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
    };

    return loadImpl(filename.string(), result, doc, offset);
}

ref<Object> loadImpl(const std::string &id,
                     const pugi::xml_parse_result &result,
                     const pugi::xml_document &doc,
                     const std::function<std::string(ptrdiff_t)> &offset) {

    if (!result) /* There was a parser / file IO error */
        Throw("Error while parsing \"%s\": %s (at %s)", id,
              result.description(), offset(result.offset));

    /* Helper function to check if attributes are fully specified */
    auto check_attributes = [&](const pugi::xml_node &node, std::set<std::string> attrs) {
        for (auto attr : node.attributes()) {
            auto it = attrs.find(attr.name());
            if (it == attrs.end())
                Throw("Error while parsing \"%s\": unexpected attribute \"%s\" "
                      "in \"%s\" at %s", id, attr.name(), node.name(),
                      offset(node.offset_debug()));
            attrs.erase(it);
        }
        if (!attrs.empty())
            Throw("Error while parsing \"%s\": missing attribute \"%s\" in "
                  "\"%s\" at %s", id, *attrs.begin(), node.name(),
                  offset(node.offset_debug()));
    };

    Eigen::Affine3f transform;

    /* Helper function to parse a Nori XML node (recursive) */
    std::function<void(pugi::xml_node &, Properties &, ETag)> parseTag = [&](
        pugi::xml_node &node, Properties &props, ETag parentTag) -> void {
        /* Skip over comments */
        if (node.type() == pugi::node_comment || node.type() == pugi::node_declaration)
            return;

        if (node.type() != pugi::node_element)
            Throw("Error while parsing \"%s\": unexpected content at %s", id,
                  offset(node.offset_debug()));

        /* Look up the name of the current element */
        auto it = tags->find(node.name());
        if (it == tags->end())
            Throw("Error while parsing \"%s\": unexpected tag \"%s\" at %s", id,
                  node.name(), offset(node.offset_debug()));
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
            Throw("Error while parsing \"%s\": root element \"%s\" must be an "
                  "object (at %s)", id, node.name(), offset(node.offset_debug()));

        if (parentIsTransform != currentIsTransformOp)
            Throw("Error while parsing \"%s\": transform nodes can only "
                  "contain transform operations (at %s)",
                  id, offset(node.offset_debug()));

        if (hasParent && !parentIsObject && !(parentIsTransform && currentIsTransformOp))
            Throw("Error while parsing \"%s\": node \"%s\" requires an object "
                  "as parent (at %s)", id, node.name(), offset(node.offset_debug()));

        if (std::string(node.name()) == "scene")
            node.append_attribute("type") = "scene";
        else if (tag == ETransform)
            transform.setIdentity();

        try {
            switch (tag) {
                case EObject: {
                        auto it2 = tagClass->find(node.name());
                        if (it2 == tagClass->end())
                            Throw("Internal error: could not find class for "
                                  "tag \"%s\"", node.name());

                        check_attributes(node, { "type" });

                        Properties propsNested(node.attribute("type").value());
                        for (pugi::xml_node &ch: node.children())
                            parseTag(ch, propsNested, tag);

                        /* This is an object, first instantiate it */
                        props.setObject("asdf", PluginManager::getInstance()->createObject(
                            it2->second, propsNested)); // XXX
                    }
                    break;
                case EString: {
                        check_attributes(node, { "name", "value" });
                        props.setString(node.attribute("name").value(), node.attribute("value").value());
                    }
                    break;
                case EFloat: {
                        check_attributes(node, { "name", "value" });
                        props.setFloat(node.attribute("name").value(), std::stof(node.attribute("value").value()));
                    }
                    break;
                case EInteger: {
                        check_attributes(node, { "name", "value" });
                        props.setInteger(node.attribute("name").value(), std::stoi(node.attribute("value").value()));
                    }
                    break;
                case EBoolean: {
                        check_attributes(node, { "name", "value" });
                        //props.setBoolean(node.attribute("name").value(), toBool(node.attribute("value").value()));
                    }
                    break;
                case EPoint: {
                        check_attributes(node, { "name", "value" });
                        //props.setPoint(node.attribute("name").value(), Point3f(toVector3f(node.attribute("value").value())));
                    }
                    break;
                case EVector: {
                        check_attributes(node, { "name", "value" });
                        //props.setVector(node.attribute("name").value(), Vector3f(toVector3f(node.attribute("value").value())));
                    }
                    break;
                case EColor: {
                        check_attributes(node, { "name", "value" });
                        //props.setColor(node.attribute("name").value(), Color3f(toVector3f(node.attribute("value").value()).array()));
                    }
                    break;
                case ETransform: {
                        check_attributes(node, { "name" });
                        //props.setTransform(node.attribute("name").value(), transform.matrix());
                    }
                    break;
                case ETranslate: {
                        check_attributes(node, { "value" });
                        //Eigen::Vector3f v = toVector3f(node.attribute("value").value());
                        //transform = Eigen::Translation<float, 3>(v.x(), v.y(), v.z()) * transform;
                    }
                    break;
                case EMatrix: {
                        check_attributes(node, { "value" });
                        std::vector<std::string> tokens = string::tokenize(node.attribute("value").value());
                        if (tokens.size() != 16)
                            Throw("matrix: expected 16 values");
                        Eigen::Matrix4f matrix;
                        for (int i=0; i<4; ++i)
                            for (int j=0; j<4; ++j)
                                matrix(i, j) = std::stof(tokens[i*4+j]);
                        transform = Eigen::Affine3f(matrix) * transform;
                    }
                    break;
                case EScale: {
                        check_attributes(node, { "value" });
                        //Eigen::Vector3f v = toVector3f(node.attribute("value").value());
                        //transform = Eigen::DiagonalMatrix<float, 3>(v) * transform;
                    }
                    break;
                case ERotate: {
                        check_attributes(node, { "angle", "axis" });
                        //float angle = math::degToRad(std::stof(node.attribute("angle").value()));
                        //Eigen::Vector3f axis = toVector3f(node.attribute("axis").value());
                        //transform = Eigen::AngleAxis<float>(angle, axis) * transform;
                    }
                    break;
                case ELookAt: {
                        check_attributes(node, { "origin", "target", "up" });
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
        } catch (const std::exception &e) {
            Throw("Error while parsing \"%s\": %s (at %s)", id, e.what(),
                  offset(node.offset_debug()));
        }
    };

    Properties prop;
    parseTag(*doc.begin(), prop, EInvalid);

    return nullptr;
}

NAMESPACE_END(xml)
NAMESPACE_END(mitsuba)
