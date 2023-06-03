#pragma once

#include "LoaderContext.h"

namespace IG {
/// @brief Representation of an entity in the scene
/// The framework does not keep all the entities in memory while loading,
/// only ones useful for other parts in the framework (e.g., area lights)
struct Entity {
    size_t ID;
    Transformf Transform;
    std::string Name;
    uint32 ShapeID;
    uint32 MatID;
    std::string BSDF;

    inline Matrix34f computeLocalMatrix() const
    {
        return Transform.inverse().matrix().block<3, 4>(0, 0);
    }
    inline Matrix34f computeGlobalMatrix() const
    {
        return Transform.matrix().block<3, 4>(0, 0);
    }
    inline Matrix3f computeGlobalNormalMatrix() const
    {
        return Transform.matrix().block<3, 3>(0, 0).inverse().transpose();
    }
};

class LoaderEntity {
public:
    void prepare(const LoaderContext& ctx);
    bool load(LoaderContext& ctx);

    [[nodiscard]] inline size_t entityCount() const { return mEntityCount; }

    [[nodiscard]] std::optional<Entity> getEmissiveEntity(const std::string& name) const;

private:
    size_t mEntityCount = 0;
    std::unordered_map<std::string, Entity> mEmissiveEntities;
};
} // namespace IG