#pragma once

#include "LoaderContext.h"

namespace IG {
class ShadingTree;
struct LoaderResult;
class LoaderLight {
public:
    void prepare(LoaderContext& ctx);
    std::string generate(ShadingTree& tree, bool skipArea);
    std::filesystem::path generateLightSelectionCDF(LoaderContext& ctx);

    inline std::shared_ptr<Parser::Object> getByID(size_t id) const { return mOrderedLights.at(id).second; }

    inline bool hasAreaLights() const { return !mAreaLights.empty(); };
    inline bool isAreaLight(const std::string& entity_name) const { return mAreaLights.count(entity_name) > 0; }
    inline size_t getAreaLightID(const std::string& entity_name) const { return mAreaLights.at(entity_name); }

    inline size_t lightCount() const { return mOrderedLights.size(); }
    inline size_t embeddedLightCount() const { return mSimplePointLightCounter + mSimpleAreaLightCounter; }
    inline size_t areaLightCount() const { return mAreaLights.size(); }

private:
    void sortLights(LoaderContext& ctx);
    void setupAreaLights();
    void embedLights(LoaderContext& ctx);

    std::vector<std::pair<std::string, std::shared_ptr<Parser::Object>>> mOrderedLights;
    size_t mSimplePointLightCounter = 0;
    size_t mSimpleAreaLightCounter  = 0;

    std::unordered_map<std::string, size_t> mAreaLights; // Name of entities being area lights
};
} // namespace IG