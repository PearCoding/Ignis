#pragma once

#include "LoaderContext.h"

namespace IG {
class Light;
class ShadingTree;
struct LoaderResult;
class LoaderLight {
public:
    void prepare(const LoaderContext& ctx);

    void setup(LoaderContext& ctx);
    std::string generate(ShadingTree& tree, bool skipFinite);
    std::filesystem::path generateLightSelectionCDF(ShadingTree& ctx);

    inline bool isAreaLight(const std::string& entity_name) const { return mEmissiveEntities.count(entity_name) > 0; }
    inline size_t getAreaLightID(const std::string& entity_name) const { return mAreaLights.at(entity_name); }

    inline size_t lightCount() const { return mInfiniteLights.size() + mFiniteLights.size(); }
    inline size_t embeddedLightCount() const { return isEmbedding() ? mTotalEmbedCount : 0; }
    inline bool isEmbedding() const { return mTotalEmbedCount >= 10; }
    inline size_t areaLightCount() const { return mAreaLights.size(); }

private:
    void findEmissiveEntities(const LoaderContext& ctx);

    void loadLights(LoaderContext& ctx);
    void setupEmbedClasses();
    void sortLights();
    void setupLightIDs();
    void setupAreaLights();
    void embedLights(ShadingTree& tree);

    std::vector<std::shared_ptr<Light>> mInfiniteLights;
    std::vector<std::shared_ptr<Light>> mFiniteLights;

    std::unordered_set<std::string> mEmissiveEntities; // Set of entities emitting energy into the scene, not yet associated with a light

    std::unordered_map<std::string, size_t> mEmbedClassCounter;
    size_t mTotalEmbedCount;
    std::unordered_map<std::string, size_t> mAreaLights; // Name of entities being area lights
};
} // namespace IG