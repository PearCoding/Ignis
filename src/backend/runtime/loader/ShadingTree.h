#pragma once

#include "Transpiler.h"
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
        std::string TexName;
        std::unordered_map<std::string, std::string> Parameters;
    };

public:
    explicit ShadingTree(LoaderContext& ctx);

    /// Register new closure, can be empty if not a texture
    bool beginClosure(const std::string& texName = {});
    void endClosure();

    void addNumber(const std::string& name, const Parser::Object& obj, float def = 0, bool hasDef = true);
    void addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def = Vector3f::Zero(), bool hasDef = true);
    void addVector(const std::string& name, const Parser::Object& obj, const Vector3f& def = Vector3f::Zero(), bool hasDef = true);
    void addTexture(const std::string& name, const Parser::Object& obj, bool hasDef = true);

    std::string generateUniqueID(const std::string& name);

    void registerTextureUsage(const std::string& name);
    std::string pullHeader();
    std::string getInline(const std::string& name);
    bool isPureTexture(const std::string& name);
    inline bool hasParameter(const std::string& name) const { return currentClosure().Parameters.count(name) > 0; }

    inline LoaderContext& context() { return mContext; }
    inline const LoaderContext& context() const { return mContext; }

    /// Use this function to mark the loading process as failed if it can not be saved in other means
    void signalError();

private:
    inline const Closure& currentClosure() const { return mClosures.back(); }
    inline Closure& currentClosure() { return mClosures.back(); }

    std::string handleTexture(const std::string& prop_name, const std::string& expr, bool needColor);

    LoaderContext& mContext;

    std::vector<std::string> mHeaderLines; // The order matters
    std::unordered_set<std::string> mLoadedTextures;
    std::unordered_set<std::string> mPureTextures;

    std::vector<Closure> mClosures;

    Transpiler mTranspiler;
    std::unordered_map<std::string, size_t> mIDMap; // Used to anonymize names for better caching
};
} // namespace IG