#pragma once

#include "LoaderContext.h"

#include <unordered_set>

namespace IG {
class Light;
class ShadingTree;
class LoaderLight {
public:
    void prepare(const LoaderContext& ctx);

    void setup(LoaderContext& ctx);
    [[nodiscard]] std::string generate(ShadingTree& tree, bool skipFinite);

    /// Will generate a variable `light_selector` for light selection purposes
    [[nodiscard]] std::string generateLightSelector(std::string type, ShadingTree& tree);

    [[nodiscard]] inline bool isAreaLight(const std::string& entity_name) const { return mEmissiveEntities.count(entity_name) > 0; }
    [[nodiscard]] inline size_t getAreaLightID(const std::string& entity_name) const { return mAreaLights.at(entity_name); }

    [[nodiscard]] inline size_t lightCount() const { return finiteLightCount() + infiniteLightCount(); }
    [[nodiscard]] inline size_t embeddedLightCount() const { return isEmbedding() ? mTotalEmbedCount : 0; }
    [[nodiscard]] inline size_t finiteLightCount() const { return mFiniteLights.size(); }
    [[nodiscard]] inline size_t infiniteLightCount() const { return mInfiniteLights.size(); }
    [[nodiscard]] inline bool isEmbedding() const { return mTotalEmbedCount >= 10; }
    [[nodiscard]] inline size_t areaLightCount() const { return mAreaLights.size(); }

private:
    void findEmissiveEntities(const LoaderContext& ctx);

    void loadLights(LoaderContext& ctx);
    void setupEmbedClasses();
    void sortLights();
    void setupInfiniteLightIDs();
    void setupFiniteLightIDs();
    void setupAreaLights();
    void embedLights(ShadingTree& tree);

    [[nodiscard]] std::string generateInfinite(ShadingTree& tree);
    [[nodiscard]] std::string generateFinite(ShadingTree& tree);

    [[nodiscard]] Path generateLightSelectionCDF(ShadingTree& tree);

    std::vector<std::shared_ptr<Light>> mInfiniteLights;
    std::vector<std::shared_ptr<Light>> mFiniteLights;

    std::unordered_set<std::string> mEmissiveEntities; // Set of entities emitting energy into the scene, not yet associated with a light

    std::unordered_map<std::string, size_t> mEmbedClassCounter;
    size_t mTotalEmbedCount;
    std::unordered_map<std::string, size_t> mAreaLights; // Name of entities being area lights
};
} // namespace IG