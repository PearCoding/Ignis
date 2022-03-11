#pragma once

#include "IG_Config.h"
#include <unordered_map>
#include <unordered_set>

namespace IG {
namespace Parser {
class Object;
class Property;
} // namespace Parser

struct LoaderContext;
class ShadingTree {
private:
    struct Closure {
        std::unordered_map<std::string, std::string> Parameters;
    };

public:
    enum InlineMode {
        IM_Bsdf = 0,
        IM_Light,
    };

    explicit ShadingTree(LoaderContext& ctx, const std::string& prefix = "");

    void beginClosure();
    void endClosure();

    void addNumber(const std::string& name, const Parser::Object& obj, float def = 0, bool hasDef = true, InlineMode mode = IM_Bsdf);
    void addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def = Vector3f::Zero(), bool hasDef = true, InlineMode mode = IM_Bsdf);
    void addTexture(const std::string& name, const Parser::Object& obj, bool hasDef = true, InlineMode mode = IM_Bsdf);

    std::string pullHeader();
    std::string getInline(const std::string& name) const;
    inline bool hasParameter(const std::string& name) const { return currentClosure().Parameters.count(name) > 0; }

    inline LoaderContext& context() { return mContext; }
    inline const LoaderContext& context() const { return mContext; }

private:
    inline const Closure& currentClosure() const { return mClosures.back(); }
    inline Closure& currentClosure() { return mClosures.back(); }

    std::string lookupTexture(const std::string& name, InlineMode mode, bool needColor = true);

    LoaderContext& mContext;

    const std::string mPrefix;
    std::vector<std::string> mHeaderLines; // The order matters
    std::unordered_set<std::string> mLoadedTextures;

    std::vector<Closure> mClosures;
};
} // namespace IG