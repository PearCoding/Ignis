#pragma once

#include "Image.h"
#include "RuntimeSettings.h"
#include "Transpiler.h"

#include <unordered_map>
#include <unordered_set>

namespace IG {
class SceneObject;
class SceneProperty;

namespace _details {
// Embed behaviour for constant values. References to other nodes will us override the property embed type
enum class EmbedType {
    Dynamic = 0, // Always uses the local registry to pull current value
    Structural,  // Never uses the local registry for the value
    Default      // Depending on internal state the value will be pulled from local registry or embedded directly
};
struct NumberOptions {
    _details::EmbedType EmbedType = _details::EmbedType::Default;
    bool SpecializeZero           = false; // Will embed if constant zero
    bool SpecializeOne            = false; // Will embed if constant one

    static constexpr NumberOptions Dynamic() { return NumberOptions{ EmbedType::Dynamic, false, false }; }
    static constexpr NumberOptions Structural() { return NumberOptions{ EmbedType::Structural, true, true }; }
    static constexpr NumberOptions Full() { return NumberOptions{ EmbedType::Default, true, true }; }
    static constexpr NumberOptions Zero() { return NumberOptions{ EmbedType::Default, true, false }; }
    static constexpr NumberOptions One() { return NumberOptions{ EmbedType::Default, false, true }; }
    static constexpr NumberOptions None() { return NumberOptions{ EmbedType::Default, false, false }; }
};
struct ColorOptions {
    _details::EmbedType EmbedType = _details::EmbedType::Default;
    bool SpecializeBlack          = false; // Will embed if constant black
    bool SpecializeWhite          = false; // Will embed if constant white

    static constexpr ColorOptions Dynamic() { return ColorOptions{ EmbedType::Dynamic, false, false }; }
    static constexpr ColorOptions Structural() { return ColorOptions{ EmbedType::Structural, true, true }; }
    static constexpr ColorOptions White() { return ColorOptions{ EmbedType::Default, false, true }; }
    static constexpr ColorOptions Black() { return ColorOptions{ EmbedType::Default, true, false }; }
    static constexpr ColorOptions Full() { return ColorOptions{ EmbedType::Default, true, true }; }
    static constexpr ColorOptions None() { return ColorOptions{ EmbedType::Default, false, false }; }
};
struct VectorOptions {
    _details::EmbedType EmbedType = _details::EmbedType::Default;
    bool SpecializeZero           = false; // Will embed if constant zero vector
    bool SpecializeOne            = false; // Will embed if constant one vector
    bool SpecializeUnit           = false; // Will embed if constant unit vector ([1,0,0], [0,1,0] or [0,0,1])

    static constexpr VectorOptions Dynamic() { return VectorOptions{ EmbedType::Dynamic, false, false, false }; }
    static constexpr VectorOptions Structural() { return VectorOptions{ EmbedType::Structural, true, true, true }; }
    static constexpr VectorOptions FullPlain() { return VectorOptions{ EmbedType::Default, true, true, false }; }
    static constexpr VectorOptions Full() { return VectorOptions{ EmbedType::Default, true, true, true }; }
    static constexpr VectorOptions None() { return VectorOptions{ EmbedType::Default, false, false, false }; }
};
struct TextureOptions {
    _details::EmbedType EmbedType = _details::EmbedType::Default;

    static constexpr TextureOptions Dynamic() { return TextureOptions{ EmbedType::Dynamic }; }
    static constexpr TextureOptions Structural() { return TextureOptions{ EmbedType::Structural }; }
    static constexpr TextureOptions Default() { return TextureOptions{ EmbedType::Default }; }
};
template <typename T>
struct GenericBakeOutput {
    T Value;
    bool WasConstant;
    static GenericBakeOutput AsConstant(const T& v) { return GenericBakeOutput<T>{ v, true }; }
    static GenericBakeOutput AsVarying(const T& v) { return GenericBakeOutput<T>{ v, false }; }
};
using BakeOutputNumber = GenericBakeOutput<float>;
using BakeOutputColor  = GenericBakeOutput<Vector3f>;
struct GenericBakeOptions {
    bool SkipTextures; // Will not bake parameters which are beyond simple constants
    static constexpr GenericBakeOptions Default() { return GenericBakeOptions{ false }; }
    static constexpr GenericBakeOptions OnlyConstants() { return GenericBakeOptions{ true }; }
};
struct TextureBakeOptions { // The bake operation might still return width and height 1 for constants or other simple expressions, completely ignoring max & min settings
    size_t MaxWidth;        // Maximum width if expression is a texture. 0 disables maximum
    size_t MaxHeight;       // Maximum height if expression is a texture. 0 disables maximum
    size_t MinWidth;        // Minimum width if expression is a texture
    size_t MinHeight;       // Minimum height if expression is a texture
    bool SkipConstant;      // Will not bake parameters which are simple constants
    static constexpr TextureBakeOptions Default() { return TextureBakeOptions{ 0, 0, 1, 1, false }; }
};
} // namespace _details

class LoaderContext;
class ShaderBuilder;
class ShadingTree {
private:
    struct Closure {
        std::string Name;
        std::string ID;
        std::unordered_map<std::string, std::string> Parameters;
    };

public:
    // Thanks to this https://stackoverflow.com/questions/53408962/try-to-understand-compiler-error-message-default-member-initializer-required-be
    // we do have to use this workaround
    using EmbedType      = _details::EmbedType;
    using NumberOptions  = _details::NumberOptions;
    using ColorOptions   = _details::ColorOptions;
    using VectorOptions  = _details::VectorOptions;
    using TextureOptions = _details::TextureOptions;

    using BakeOutputNumber   = _details::BakeOutputNumber;
    using BakeOutputColor    = _details::BakeOutputColor;
    using GenericBakeOptions = _details::GenericBakeOptions;
    using TextureBakeOptions = _details::TextureBakeOptions;

    ShadingTree(LoaderContext& ctx, ShaderBuilder& builder);

    /// Register new closure, can be empty if not a texture
    void beginClosure(const std::string& name);
    void endClosure();

    void addNumber(const std::string& name, SceneObject& obj, const std::optional<float>& def = std::make_optional<float>(0.0f), const NumberOptions& options = NumberOptions::Full());
    void addColor(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def = std::make_optional<Vector3f>(Vector3f::Zero()), const ColorOptions& options = ColorOptions::Full());
    void addVector(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def = std::make_optional<Vector3f>(Vector3f::Zero()), const VectorOptions& options = VectorOptions::Full());
    void addTexture(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def = std::make_optional<Vector3f>(Vector3f::Zero()), const TextureOptions& options = TextureOptions::Default());

    BakeOutputNumber computeNumber(const std::string& name, SceneObject& obj, float def = 0, const GenericBakeOptions& options = GenericBakeOptions::Default());
    BakeOutputColor computeColor(const std::string& name, SceneObject& obj, const Vector3f& def = Vector3f::Zero(), const GenericBakeOptions& options = GenericBakeOptions::Default());

    using BakeOutputTexture = std::optional<std::shared_ptr<Image>>;
    BakeOutputTexture bakeTexture(const std::string& name, SceneObject& obj, const std::optional<Vector3f>& def = std::make_optional<Vector3f>(Vector3f::Zero()), const TextureBakeOptions& options = TextureBakeOptions::Default());

    /// Compute minimum resolution of a texture for the given property. Only accounts image textures, not checkerboard or noise patterns. If no image texture is used the function will return (1,1)
    std::pair<size_t, size_t> computeTextureResolution(const std::string& name, SceneObject& obj);
    std::pair<size_t, size_t> computeTextureResolution(const std::string& name, const Transpiler::Result& result);

    inline std::string currentClosureID() const { return currentClosure().ID; }
    std::string getClosureID(const std::string& name);

    void registerTextureUsage(const std::string& name);
    std::string getInline(const std::string& name);
    inline bool hasParameter(const std::string& name) const { return currentClosure().Parameters.count(name) > 0; }

    inline LoaderContext& context() { return mContext; }
    inline const LoaderContext& context() const { return mContext; }

    inline void setSpecialization(RuntimeOptions::SpecializationMode mode) { mSpecialization = mode; }
    inline RuntimeOptions::SpecializationMode specializationMode() const { return mSpecialization; }

    /// Use this function to mark the loading process as failed if it can not be saved with other means
    void signalError();

    inline ShaderBuilder& shader() { return mBuilder; }

private:
    inline const Closure& currentClosure() const { return mClosures.back(); }
    inline Closure& currentClosure() { return mClosures.back(); }

    void setupGlobalParameters();
    float handleGlobalParameterNumber(const std::string& name, const SceneProperty& prop);
    Vector3f handleGlobalParameterVector(const std::string& name, const SceneProperty& prop);
    Vector4f handleGlobalParameterColor(const std::string& name, const SceneProperty& prop);

    std::string handlePropertyNumber(const std::string& name, const SceneProperty& prop, const NumberOptions& options);

    std::string handleTexture(const std::string& prop_name, const std::string& expr, bool needColor);
    std::string acquireNumber(const std::string& prop_name, float number, const NumberOptions& options);
    std::string acquireColor(const std::string& prop_name, const Vector3f& color, const ColorOptions& options);
    std::string acquireVector(const std::string& prop_name, const Vector3f& vec, const VectorOptions& options);
    bool checkIfEmbed(float val, const NumberOptions& options) const;
    bool checkIfEmbed(const Vector3f& color, const ColorOptions& options) const;
    bool checkIfEmbed(const Vector3f& vec, const VectorOptions& options) const;

    std::pair<size_t, size_t> computeTextureResolution(const std::string& name, const std::string& expr);
    BakeOutputTexture bakeTextureExpression(const std::string& name, const std::string& expr, const TextureBakeOptions& options);
    BakeOutputColor bakeTextureExpressionAverage(const std::string& name, const std::string& expr, const Vector3f& def, const GenericBakeOptions& options);
    Vector3f computeConstantColor(const std::string& name, const Transpiler::Result& result);
    Image computeImage(const std::string& name, const Transpiler::Result& result, const TextureBakeOptions& options);
    void loadTexture(const std::string& tex_name);

    LoaderContext& mContext;
    ShaderBuilder& mBuilder;

    std::unordered_set<std::string> mLoadedTextures;

    std::vector<Closure> mClosures;

    Transpiler mTranspiler;
    std::unordered_map<std::string, size_t> mIDMap; // Used to anonymize names for better caching

    RuntimeOptions::SpecializationMode mSpecialization;
};
} // namespace IG