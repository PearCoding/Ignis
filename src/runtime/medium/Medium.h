#pragma once

#include "SceneObjectProxy.h"

namespace IG {
class ShadingTree;
class SceneObject;

class Medium : public SceneObjectProxy {
public:
    inline Medium(const std::string& name, const std::string& type)
        : SceneObjectProxy(name, type)
        , mID(0)
        , mReferenceEntityID(0)
        , mReferenceEntityIDSet(false)
    {
    }

    virtual ~Medium() = default;

    struct SerializationInput {
        size_t ID;
        std::ostream& Stream;
        ShadingTree& Tree;
    };
    virtual void serialize(const SerializationInput& input) const = 0;

    [[nodiscard]] inline size_t id() const { return mID; }
    inline void setID(size_t id) { mID = id; }

    [[nodiscard]] inline std::string referenceEntity() const { return mReferenceEntity; }
    inline void setReferenceEntity(const std::string& name) { mReferenceEntity = name; } // Will be set by LoaderEntity if not already present
    [[nodiscard]] inline bool hasReferenceEntity() const { return !mReferenceEntity.empty(); }

    [[nodiscard]] inline size_t referenceEntityID() const { return mReferenceEntityID; } // Will be set later in LoaderEntity
    inline void setReferenceEntityID(size_t id)
    {
        mReferenceEntityID    = id;
        mReferenceEntityIDSet = true;
    }
    [[nodiscard]] inline bool isReferenceEntityIDSet() const { return mReferenceEntityIDSet; }

protected:
    void handleReferenceEntity(SceneObject& obj);
    /// Will generate a function fn () -> PointMapperSet and return the name of it
    std::string generateReferencePMS(const SerializationInput& input) const; 

private:
    size_t mID;
    size_t mReferenceEntityID;
    bool mReferenceEntityIDSet;
    std::string mReferenceEntity;
};
} // namespace IG