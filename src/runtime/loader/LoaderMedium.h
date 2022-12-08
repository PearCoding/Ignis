#pragma once

#include "LoaderContext.h"

namespace IG {
class Medium;
class ShadingTree;
class LoaderMedium {
public:
    void prepare(LoaderContext& ctx);

    [[nodiscard]] std::string generate(ShadingTree& tree);

    [[nodiscard]] inline bool hasParticipatingMedia() const { return mediumCount() > 0; }
    [[nodiscard]] inline size_t mediumCount() const { return mMedia.size(); }

    void handleReferenceEntity(const std::string& mediumInnerName, const std::string& entity_name, size_t id);

private:
    std::unordered_map<std::string, std::shared_ptr<Medium>> mMedia;

    void loadMedia(LoaderContext& ctx);
    void generateReferencePMS(ShadingTree& tree);
};
} // namespace IG