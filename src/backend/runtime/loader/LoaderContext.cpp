#include "LoaderContext.h"
#include "Image.h"
#include "Logger.h"
#include "serialization/VectorSerializer.h"
#include "table/SceneDatabase.h"

namespace IG {

using namespace Parser;

Vector3f LoaderContext::extractColor(const Parser::Object& obj, const std::string& propname, const Vector3f& def) const
{
    auto prop = obj.property(propname);
    if (prop.isValid()) {
        switch (prop.type()) {
        case PT_INTEGER:
            return Vector3f::Ones() * prop.getInteger();
        case PT_NUMBER:
            return Vector3f::Ones() * prop.getNumber();
        case PT_VECTOR3:
            return prop.getVector3();
        case PT_STRING: {
            std::string name = obj.property(propname).getString();
            IG_LOG(L_WARNING) << "[TODO] Replacing texture '" << name << "' by average color" << std::endl;
            if (TextureBuffer.count(name)) {
                uint32 id = TextureBuffer.at(name);
                return TextureAverages.at(id);
            } else {
                IG_LOG(L_ERROR) << "No texture '" << name << "' found!" << std::endl;
                return def;
            }
        }
        default:
            IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
            return def;
        }
    } else {
        return def;
    }
}

float LoaderContext::extractIOR(const Parser::Object& obj, const std::string& propname, float def) const
{
    auto prop = obj.property(propname);
    if (prop.isValid()) {
        switch (prop.type()) {
        case PT_INTEGER:
            return prop.getInteger();
        case PT_NUMBER:
            return prop.getNumber();
        case PT_VECTOR3:
            return prop.getVector3().mean();
        case PT_STRING: {
            std::string name = obj.property(propname).getString();
            IG_LOG(L_WARNING) << "[TODO] Replacing texture '" << name << "' by average color" << std::endl;
            if (TextureBuffer.count(name)) {
                uint32 id = TextureBuffer.at(name);
                return TextureAverages.at(id).mean();
            } else {
                IG_LOG(L_ERROR) << "No texture '" << name << "' found!" << std::endl;
                return def;
            }
        }
        default:
            IG_LOG(L_WARNING) << "Unknown property type for '" << propname << "'" << std::endl;
            return def;
        }
    } else {
        return def;
    }
}

std::filesystem::path LoaderContext::handlePath(const std::filesystem::path& path, const Parser::Object& obj) const
{
    if (path.is_absolute())
        return path;

    if (!obj.baseDir().empty()) {
        const auto p = obj.baseDir() / path;
        if (std::filesystem::exists(p))
            return std::filesystem::canonical(p);
    }

    if (!FilePath.empty()) {
        const auto p = FilePath.parent_path() / path;
        if (std::filesystem::exists(p))
            return std::filesystem::canonical(p);
    }

    return std::filesystem::canonical(path);
}

} // namespace IG