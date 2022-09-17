#pragma once

#include "LoaderContext.h"
#include "shape/PlaneShape.h"
#include "shape/Shape.h"
#include "shape/ShapeProvider.h"
#include "shape/TriShape.h"

#include <mutex>

namespace IG {
struct LoaderResult;

class LoaderShape {
public:
    void prepare(const LoaderContext& ctx);
    bool load(LoaderContext& ctx, LoaderResult& result);

    // Note: The following accessors should not be called inside the ShapeProvider, as they are not thread-safe
    [[nodiscard]] inline bool hasShape(const std::string& name) const { return mIDs.count(name) > 0; }
    [[nodiscard]] inline uint32 getShapeID(const std::string& name) const { return mIDs.at(name); }
    [[nodiscard]] inline bool isTriShape(uint32 id) const { return mTriShapes.count(id) > 0; }
    [[nodiscard]] inline bool isTriShape(const std::string& name) const { return isTriShape(getShapeID(name)); }
    [[nodiscard]] inline bool isPlaneShape(uint32 id) const { return mPlaneShapes.count(id) > 0; }
    [[nodiscard]] inline bool isPlaneShape(const std::string& name) const { return isPlaneShape(getShapeID(name)); }

    [[nodiscard]] inline const Shape& getShape(uint32 id) const { return mShapes.at(id); }
    [[nodiscard]] inline const TriShape& getTriShape(uint32 id) const { return mTriShapes.at(id); }
    [[nodiscard]] inline const PlaneShape& getPlaneShape(uint32 id) const { return mPlaneShapes.at(id); }

    [[nodiscard]] inline size_t shapeCount() const { return mShapes.size(); }
    [[nodiscard]] inline size_t triShapeCount() const { return mTriShapes.size(); }
    [[nodiscard]] inline size_t planeShapeCount() const { return mPlaneShapes.size(); }

    // Note: The following functions can be called inside the ShapeProvider, as they are thread-safe
    uint32 addShape(const std::string& name, const Shape& shape);
    void addTriShape(uint32 id, const TriShape& shape);
    void addPlaneShape(uint32 id, const PlaneShape& shape);

    [[nodiscard]] inline const std::unordered_map<std::string, std::unique_ptr<ShapeProvider>>& providers() const { return mShapeProviders; }

private:
    std::unordered_map<std::string, std::unique_ptr<ShapeProvider>> mShapeProviders;

    std::vector<Shape> mShapes;
    std::unordered_map<std::string, uint32> mIDs;
    std::unordered_map<uint32, TriShape> mTriShapes;     // Used for special optimization
    std::unordered_map<uint32, PlaneShape> mPlaneShapes; // Used for special optimization

    std::mutex mAccessMutex;
};
} // namespace IG