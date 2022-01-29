#pragma once

#include "LoaderContext.h"
#include <unordered_map>

namespace IG {
namespace Parser {
class Object;
class Property;
} // namespace Parser

class ShadingTree {
public:
    ShadingTree(const std::string& prefix = "");

    void addNumber(const std::string& name, const LoaderContext& ctx, const Parser::Object& obj, float def = 0, bool hasDef = true);
    void addColor(const std::string& name, const LoaderContext& ctx, const Parser::Object& obj, const Vector3f& def = Vector3f::Zero(), bool hasDef = true);
    void addTexture(const std::string& name, const LoaderContext& ctx, const Parser::Object& obj, bool hasDef = true);

    std::string pullHeader();
    std::string getInline(const std::string& name) const;

    inline bool hasParameter(const std::string& name) const { return mParameters.count(name) > 0; }

    // Use only tex_coords and not the full surf structure!
    inline bool isOnlyUsingCoords() const { return mUseOnlyCoords; }
    inline void onlyUseCoords(bool b) { mUseOnlyCoords = b; }

private:
    std::string lookupTexture(const std::string& name, const LoaderContext& ctx, bool needColor = true);

    const std::string mPrefix;
    std::vector<std::string> mHeaderLines; // The order matters
    std::unordered_set<std::string> mLoadedTextures;
    std::unordered_map<std::string, std::string> mParameters;

    bool mUseOnlyCoords;
};
} // namespace IG