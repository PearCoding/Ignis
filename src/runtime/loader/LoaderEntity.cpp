#include "LoaderEntity.h"
#include "Loader.h"
#include "LoaderLight.h"
#include "LoaderShape.h"
#include "Logger.h"
#include "bvh/SceneBVHAdapter.h"
#include "serialization/VectorSerializer.h"

#include <chrono>

namespace IG {
using namespace Parser;
void LoaderEntity::prepare(const LoaderContext& ctx)
{
    IG_UNUSED(ctx);
}

template <size_t N>
inline static void setup_bvh(std::vector<EntityObject>& input, SceneBVH& bvh)
{
    using Node = typename BvhNEnt<N>::Node;
    std::vector<Node> nodes;
    std::vector<EntityLeaf1> objs;
    build_scene_bvh<N>(nodes, objs, input);

    bvh.Nodes.resize(sizeof(Node) * nodes.size());
    std::memcpy(bvh.Nodes.data(), nodes.data(), bvh.Nodes.size());
    bvh.Leaves.resize(sizeof(EntityLeaf1) * objs.size());
    std::memcpy(bvh.Leaves.data(), objs.data(), bvh.Leaves.size());
}

bool LoaderEntity::load(LoaderContext& ctx, LoaderResult& result)
{
    // Fill entity list
    ctx.Environment.SceneBBox = BoundingBox::Empty();

    const auto start1 = std::chrono::high_resolution_clock::now();

    auto& entityTable = result.Database.FixTables["entities"];
    entityTable.reserve(ctx.Scene.entities().size() * 48);

    std::unordered_map<ShapeProvider*, std::vector<EntityObject>> in_objs;
    mEntityCount = 0;
    for (const auto& pair : ctx.Scene.entities()) {
        const auto child = pair.second;

        // Query shape
        const std::string shapeName = child->property("shape").getString();
        if (shapeName.empty()) {
            IG_LOG(L_ERROR) << "Entity " << pair.first << " has no shape" << std::endl;
            continue;
        } else if (!ctx.Shapes->hasShape(shapeName)) {
            IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown shape " << shapeName << std::endl;
            continue;
        }

        const uint32 shapeID = ctx.Shapes->getShapeID(shapeName);

        // Query bsdf
        const std::string bsdfName = child->property("bsdf").getString();
        if (bsdfName.empty()) {
            IG_LOG(L_ERROR) << "Entity " << pair.first << " has no bsdf" << std::endl;
            continue;
        } else if (!ctx.Scene.bsdf(bsdfName)) {
            IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown bsdf " << bsdfName << std::endl;
            continue;
        }

        // Query medium interface
        const std::string mediumInnerName = child->property("inner_medium").getString();
        int mediumInner                   = -1;

        if (!mediumInnerName.empty()) {
            if (!ctx.Scene.medium(mediumInnerName)) {
                IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown medium " << mediumInnerName << std::endl;
                continue;
            } else {
                const auto it = ctx.Scene.media().find(mediumInnerName);
                mediumInner   = (int)std::distance(ctx.Scene.media().begin(), it);
            }
        }

        const std::string mediumOuterName = child->property("outer_medium").getString();
        int mediumOuter                   = -1;

        if (!mediumOuterName.empty()) {
            if (!ctx.Scene.medium(mediumOuterName)) {
                IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown medium " << mediumOuterName << std::endl;
                continue;
            } else {
                const auto it = ctx.Scene.media().find(mediumOuterName);
                mediumOuter   = (int)std::distance(ctx.Scene.media().begin(), it);
            }
        }

        // Populate flags
        uint32 entity_flags = 0;
        if (child->property("camera_visible").getBool(true))
            entity_flags |= 0x1;
        if (child->property("light_visible").getBool(true))
            entity_flags |= 0x2;
        if (child->property("bounce_visible").getBool(true))
            entity_flags |= 0x4;
        if (child->property("shadow_visible").getBool(true))
            entity_flags |= 0x8;

        // Extract entity information
        Transformf transform = child->property("transform").getTransform();
        transform.makeAffine();

        const auto& shape = ctx.Shapes->getShape(shapeID);

        const Transformf invTransform = transform.inverse();
        const BoundingBox& shapeBox   = shape.BoundingBox;
        const BoundingBox entityBox   = shapeBox.transformed(transform);

        // Extend scene box
        ctx.Environment.SceneBBox.extend(entityBox);

        // Register name for lights to associate with
        uint32 materialID = 0;
        if (ctx.Lights->isAreaLight(pair.first)) {
            ctx.Environment.EmissiveEntities.insert({ pair.first, Entity{ mEntityCount, transform, pair.first, shapeID, bsdfName } });

            // It is a unique material
            materialID = (uint32)ctx.Environment.Materials.size();
            ctx.Environment.Materials.push_back(Material{ bsdfName, mediumInner, mediumOuter, pair.first });
        } else {
            Material mat{ bsdfName, mediumInner, mediumOuter, {} };
            auto it = std::find(ctx.Environment.Materials.begin(), ctx.Environment.Materials.end(), mat);
            if (it == ctx.Environment.Materials.end()) {
                materialID = (uint32)ctx.Environment.Materials.size();
                ctx.Environment.Materials.push_back(mat);
            } else {
                materialID = (uint32)std::distance(ctx.Environment.Materials.begin(), it);
            }
        }

        // Remember the entity to material
        result.Database.EntityToMaterial.push_back(materialID);

        const Eigen::Matrix<float, 3, 4> toLocal        = invTransform.matrix().block<3, 4>(0, 0);
        const Eigen::Matrix<float, 3, 4> toGlobal       = transform.matrix().block<3, 4>(0, 0);
        const Eigen::Matrix<float, 3, 3> toGlobalNormal = toGlobal.block<3, 3>(0, 0).inverse().transpose();
        const float scaleFactor                         = std::abs(toGlobalNormal.determinant());

        // Write data to dyntable
        auto& entityData = entityTable.addEntry(DefaultAlignment);
        VectorSerializer entitySerializer(entityData, false);
        entitySerializer.write(toLocal, true);        // To Local
        entitySerializer.write(toGlobal, true);       // To Global
        entitySerializer.write(toGlobalNormal, true); // To Global [Normal]
        entitySerializer.write((uint32)shapeID);
        entitySerializer.write(scaleFactor);

        // Extract information for BVH building
        EntityObject obj;
        obj.BBox     = entityBox;
        obj.Local    = invTransform.matrix();
        obj.EntityID = (int32)mEntityCount;
        obj.ShapeID  = shapeID;
        obj.User1ID  = shape.User1ID;
        obj.User2ID  = shape.User2ID;
        obj.User3ID  = shape.User3ID;
        obj.Flags    = entity_flags; // Only added to bvh

        in_objs[shape.Provider].emplace_back(obj);
        mEntityCount++;
    }

    IG_LOG(L_DEBUG) << "Storing Entities took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

    ctx.EntityCount = mEntityCount;
    if (ctx.EntityCount == 0) {
        ctx.Environment.SceneDiameter = 0;
        return true;
    }

    ctx.Environment.SceneDiameter = ctx.Environment.SceneBBox.diameter().norm();

    // Build bvh (keep in mind that this BVH has no pre-padding as in the case for shape BVHs)
    IG_LOG(L_DEBUG) << "Generating BVH for scene" << std::endl;
    const auto start2 = std::chrono::high_resolution_clock::now();
    for (auto& p : in_objs) {
        auto& bvh = result.Database.SceneBVHs[p.first->identifier()];
        if (ctx.Target.isGPU()) {
            setup_bvh<2>(p.second, bvh);
        } else if (ctx.Target.vectorWidth() < 8) {
            setup_bvh<4>(p.second, bvh);
        } else {
            setup_bvh<8>(p.second, bvh);
        }
    }
    IG_LOG(L_DEBUG) << "Building Scene BVH took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start2).count() / 1000.0f << " seconds" << std::endl;

    return true;
}

} // namespace IG