#include "ShadingTree.h"
#include "Loader.h"
#include "LoaderTexture.h"
#include "Logger.h"
#include "ShaderUtils.h"

namespace IG {
enum NodeChannel {
    NC_NONE = 0,
    NC_RED,
    NC_GREEN,
    NC_BLUE,
    NC_MEAN,
    NC_ALPHA
};
static inline std::pair<std::string, NodeChannel> escapeTextureName(const std::string& name)
{
    size_t pos = name.find_last_of('.');
    if (pos == std::string::npos)
        return { name, NC_NONE };
    else {
        std::string channelStr = name.substr(pos + 1);
        NodeChannel channel    = NC_NONE;
        if (channelStr == "r" || channelStr == "x")
            channel = NC_RED;
        else if (channelStr == "g" || channelStr == "y")
            channel = NC_GREEN;
        else if (channelStr == "b" || channelStr == "z")
            channel = NC_BLUE;
        else if (channelStr == "m")
            channel = NC_MEAN;
        else if (channelStr == "a" || channelStr == "w")
            channel = NC_ALPHA;
        else
            IG_LOG(L_WARNING) << "Unknown channel '" << channelStr << "' in node lookup '" << name << "'" << std::endl;

        return { name.substr(0, pos), channel };
    }
}

ShadingTree::ShadingTree(LoaderContext& ctx, const std::string& prefix)
    : mContext(ctx)
    , mPrefix(prefix)
{
    beginClosure();
}

void ShadingTree::addNumber(const std::string& name, const Parser::Object& obj, float def, bool hasDef, InlineMode mode)
{
    if (hasParameter(name))
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        if (!hasDef)
            return;
        inline_str = std::to_string(def);
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        inline_str = std::to_string(prop.getNumber());
        break;
    case Parser::PT_VECTOR3:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects a number but a color was given. Using average instead" << std::endl;
        inline_str = std::to_string(prop.getVector3().mean());
        break;
    case Parser::PT_STRING: {
        const auto [texName, texChannel] = escapeTextureName(prop.getString());
        std::string tex_id               = lookupTexture(texName, mode);

        switch (texChannel) {
        default:
        case NC_MEAN:
        case NC_NONE:
            if (texChannel == NC_NONE)
                IG_LOG(L_WARNING) << "Parameter '" << name << "' expects a number but a colored texture was given. Using average instead" << std::endl;
            inline_str = "color_average(" + tex_id + ")";
            break;
        case NC_RED:
            inline_str = tex_id + ".r";
            break;
        case NC_GREEN:
            inline_str = tex_id + ".g";
            break;
        case NC_BLUE:
            inline_str = tex_id + ".b";
            break;
        case NC_ALPHA:
            inline_str = tex_id + ".a";
            break;
        }
    } break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def, bool hasDef, InlineMode mode)
{
    if (hasParameter(name))
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        if (!hasDef)
            return;
        inline_str = "make_color(" + std::to_string(def.x()) + ", " + std::to_string(def.y()) + ", " + std::to_string(def.z()) + ", 1)";
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects color but only a number was given" << std::endl;
        inline_str = "make_gray_color(" + std::to_string(prop.getNumber()) + ")";
        break;
    case Parser::PT_VECTOR3: {
        Vector3f color = prop.getVector3();
        inline_str     = "make_color(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ", 1)";
    } break;
    case Parser::PT_STRING: {
        const auto [texName, texChannel] = escapeTextureName(prop.getString());
        std::string tex_id               = lookupTexture(texName, mode);

        switch (texChannel) {
        default:
        case NC_NONE:
            inline_str = tex_id;
            break;
        case NC_MEAN:
            inline_str = "make_gray_color(color_average(" + tex_id + "))";
            break;
        case NC_RED:
            inline_str = "make_gray_color(" + tex_id + ".r)";
            break;
        case NC_GREEN:
            inline_str = "make_gray_color(" + tex_id + ".g)";
            break;
        case NC_BLUE:
            inline_str = "make_gray_color(" + tex_id + ".b)";
            break;
        case NC_ALPHA:
            inline_str = "make_gray_color(" + tex_id + ".a)";
            break;
        }
    } break;
    }

    currentClosure().Parameters[name] = inline_str;
}

// Only use this if no basic color information suffices
void ShadingTree::addTexture(const std::string& name, const Parser::Object& obj, bool hasDef, InlineMode mode)
{
    if (hasParameter(name))
        IG_LOG(L_ERROR) << "Multiple use of parameter '" << name << "'" << std::endl;

    const auto prop = obj.property(name);

    std::string inline_str;
    switch (prop.type()) {
    default:
        IG_LOG(L_ERROR) << "Parameter '" << name << "' has invalid type" << std::endl;
        [[fallthrough]];
    case Parser::PT_NONE:
        if (!hasDef)
            return;
        inline_str = "make_black_texture()";
        break;
    case Parser::PT_INTEGER:
    case Parser::PT_NUMBER:
        IG_LOG(L_WARNING) << "Parameter '" << name << "' expects texture but only a number was given" << std::endl;
        inline_str = "make_constant_texture(make_gray_color(" + std::to_string(prop.getNumber()) + "))";
        break;
    case Parser::PT_VECTOR3: {
        Vector3f color = prop.getVector3();
        inline_str     = "make_constant_texture(make_color(" + std::to_string(color.x()) + ", " + std::to_string(color.y()) + ", " + std::to_string(color.z()) + ", 1))";
    } break;
    case Parser::PT_STRING: {
        const auto [texName, texChannel] = escapeTextureName(prop.getString());
        std::string tex_id               = lookupTexture(texName, mode, false);

        switch (texChannel) {
        default:
        case NC_NONE:
            inline_str = tex_id;
            break;
        // TODO: Mean
        case NC_RED:
            inline_str = "make_channel_texture(" + tex_id + ", 0)";
            break;
        case NC_GREEN:
            inline_str = "make_channel_texture(" + tex_id + ", 1)";
            break;
        case NC_BLUE:
            inline_str = "make_channel_texture(" + tex_id + ", 2)";
            break;
        case NC_ALPHA:
            inline_str = "make_channel_texture(" + tex_id + ", 3)";
            break;
        }
    } break;
    }

    currentClosure().Parameters[name] = inline_str;
}

void ShadingTree::beginClosure()
{
    mClosures.emplace_back();
}

void ShadingTree::endClosure()
{
    IG_ASSERT(mClosures.size() > 1, "Invalid closure end");
    mClosures.pop_back();
}

std::string ShadingTree::pullHeader()
{
    std::stringstream stream;
    for (const auto& lines : mHeaderLines)
        stream << lines;
    mHeaderLines.clear();
    return stream.str();
}

std::string ShadingTree::getInline(const std::string& name) const
{
    if (hasParameter(name))
        return currentClosure().Parameters.at(name);
    IG_LOG(L_ERROR) << "Trying to access unknown parameter '" << name << "'" << std::endl;
    return "";
}

std::string ShadingTree::lookupTexture(const std::string& name, InlineMode mode, bool needColor)
{
    if (mLoadedTextures.count(name) == 0) {
        const auto tex = mContext.Scene.texture(name);
        if (!tex) {
            IG_LOG(L_ERROR) << "Unknown texture '" << name << "'" << std::endl;
            return "pink";
        }

        mHeaderLines.push_back(LoaderTexture::generate(mPrefix + name, *tex, *this));
        mLoadedTextures.insert(name);
    }
    return "tex_" + ShaderUtils::escapeIdentifier(mPrefix + name) + (needColor ? (mode == IM_Light ? "(tex_coords)" : "(surf.tex_coords)") : "");
}
} // namespace IG