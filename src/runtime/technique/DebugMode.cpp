#include "DebugMode.h"
#include "StringUtils.h"

namespace IG {
static std::vector<std::pair<std::string, DebugMode>> DebugModeMap = {
    { "normal", DebugMode::Normal },
    { "tangent", DebugMode::Tangent },
    { "bitangent", DebugMode::Bitangent },
    { "geometric normal", DebugMode::GeometryNormal },
    { "local normal", DebugMode::LocalNormal },
    { "local tangent", DebugMode::LocalTangent },
    { "local bitangent", DebugMode::LocalBitangent },
    { "local geometric normal", DebugMode::LocalGeometryNormal },
    { "texture coords", DebugMode::TexCoords },
    { "prim coords", DebugMode::PrimCoords },
    { "point", DebugMode::Point },
    { "local point", DebugMode::LocalPoint },
    { "generated coords", DebugMode::GeneratedCoords },
    { "hit distance", DebugMode::HitDistance },
    { "area", DebugMode::Area },
    { "raw prim id", DebugMode::PrimIDRaw },
    { "prim id", DebugMode::PrimID },
    { "raw entity id", DebugMode::EntityIDRaw },
    { "entity id", DebugMode::EntityID },
    { "raw material id", DebugMode::MaterialIDRaw },
    { "material id", DebugMode::MaterialID },
    { "is emissive", DebugMode::IsEmissive },
    { "is specular", DebugMode::IsSpecular },
    { "is entering", DebugMode::IsEntering },
    { "check bsdf", DebugMode::CheckBSDF },
    { "albedo", DebugMode::Albedo },
    { "medium inner", DebugMode::MediumInner },
    { "medium outer", DebugMode::MediumOuter }
};

static std::unordered_map<std::string, DebugMode> genDebugModeStrToEnum()
{
    std::unordered_map<std::string, DebugMode> map;
    map.reserve(DebugModeMap.size());
    for (const auto& p : DebugModeMap)
        map[p.first] = p.second;
    return map;
}
static std::unordered_map<DebugMode, std::string> genDebugModeEnumToStr()
{
    std::unordered_map<DebugMode, std::string> map;
    map.reserve(DebugModeMap.size());
    for (const auto& p : DebugModeMap)
        map[p.second] = p.first;
    return map;
}

std::optional<DebugMode> stringToDebugMode(const std::string& name)
{
    static auto map = genDebugModeStrToEnum();

    std::string low_name = to_lowercase(name);
    if (auto it = map.find(low_name); it != map.end())
        return std::make_optional(it->second);

    return std::nullopt;
}

std::string debugModeToString(DebugMode mode)
{
    static auto map = genDebugModeEnumToStr();
    if (auto it = map.find(mode); it != map.end())
        return to_word_uppercase(it->second);

    return {};
}

std::vector<std::string> getDebugModeNames()
{
    std::vector<std::string> names;
    names.reserve(DebugModeMap.size());

    for (const auto& p : DebugModeMap)
        names.push_back(to_word_uppercase(p.first));

    return names;
}
} // namespace IG