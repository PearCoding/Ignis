#pragma once

#include "Transpiler.h"
#include <unordered_map>
#include <unordered_set>

namespace IG {
namespace Parser {
class Object;
class Property;
} // namespace Parser

// Embed behaviour for constant values. References to other nodes will us override the property embed type
enum class ShadingTreePropertyEmbedType {
    Dynamic = 0, // Always uses the local registry to pull current value
    Structural,  // Never uses the local registry for the value
    Default      // Depending on internal state the value will be pulled from local registry or embedded directly
};

struct LoaderContext;
class ShadingTree {
private:
    struct Closure {
        std::string Name;
        std::string ID;
        std::unordered_map<std::string, std::string> Parameters;
    };

public:
    explicit ShadingTree(LoaderContext& ctx);

    /// Register new closure, can be empty if not a texture
    bool beginClosure(const std::string& name);
    void endClosure();

    void addNumber(const std::string& name, const Parser::Object& obj, float def = 0, bool hasDef = true, ShadingTreePropertyEmbedType embedType = ShadingTreePropertyEmbedType::Default);
    void addColor(const std::string& name, const Parser::Object& obj, const Vector3f& def = Vector3f::Zero(), bool hasDef = true, ShadingTreePropertyEmbedType embedType = ShadingTreePropertyEmbedType::Default);
    void addVector(const std::string& name, const Parser::Object& obj, const Vector3f& def = Vector3f::Zero(), bool hasDef = true, ShadingTreePropertyEmbedType embedType = ShadingTreePropertyEmbedType::Default);
    void addTexture(const std::string& name, const Parser::Object& obj, bool hasDef = true, ShadingTreePropertyEmbedType embedType = ShadingTreePropertyEmbedType::Default);

    inline std::string currentClosureID() const { return currentClosure().ID; }
    std::string generateUniqueID(const std::string& name);

    void registerTextureUsage(const std::string& name);
    std::string pullHeader();
    std::string getInline(const std::string& name);
    bool isPureTexture(const std::string& name);
    inline bool hasParameter(const std::string& name) const { return currentClosure().Parameters.count(name) > 0; }

    inline LoaderContext& context() { return mContext; }
    inline const LoaderContext& context() const { return mContext; }

    inline void enableLocalRegistryUsage(bool b = true) { mUseLocalRegistry = b; }
    inline bool isLocalRegistryUsageEnabled() const { return mUseLocalRegistry; }

    /// Use this function to mark the loading process as failed if it can not be saved in other means
    void signalError();

private:
    inline const Closure& currentClosure() const { return mClosures.back(); }
    inline Closure& currentClosure() { return mClosures.back(); }

    std::string handleTexture(const std::string& prop_name, const std::string& expr, bool needColor);
    std::string acquireNumber(const std::string& prop_name, float number, ShadingTreePropertyEmbedType embedType);
    std::string acquireColor(const std::string& prop_name, const Vector3f& color, ShadingTreePropertyEmbedType embedType);
    std::string acquireVector(const std::string& prop_name, const Vector3f& vec, ShadingTreePropertyEmbedType embedType);
    bool checkIfEmbed(ShadingTreePropertyEmbedType embedType) const;

    LoaderContext& mContext;

    std::vector<std::string> mHeaderLines; // The order matters
    std::unordered_set<std::string> mLoadedTextures;
    std::unordered_set<std::string> mPureTextures;

    std::vector<Closure> mClosures;

    Transpiler mTranspiler;
    std::unordered_map<std::string, size_t> mIDMap; // Used to anonymize names for better caching

    bool mUseLocalRegistry;
};
} // namespace IG