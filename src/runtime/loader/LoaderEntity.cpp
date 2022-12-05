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

bool LoaderEntity::load(LoaderContext& ctx)
{
    // Fill entity list
    ctx.SceneBBox = BoundingBox::Empty();

    const auto start1 = std::chrono::high_resolution_clock::now();

    auto& entityTable = ctx.Database.FixTables["entities"];
    entityTable.reserve(ctx.Options.Scene.entities().size() * 36);

    // First group all entities to groups of unique materials
    std::vector<std::vector<std::pair<std::string, std::shared_ptr<Parser::Object>>>> material_groups;
    for (const auto& pair : ctx.Options.Scene.entities()) {
        const auto child = pair.second;

        // Query bsdf
        const std::string bsdfName = child->property("bsdf").getString();
        if (bsdfName.empty()) {
            IG_LOG(L_ERROR) << "Entity " << pair.first << " has no bsdf" << std::endl;
            continue;
        } else if (!ctx.Options.Scene.bsdf(bsdfName)) {
            IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown bsdf " << bsdfName << std::endl;
            continue;
        }

        // Query medium interface
        const std::string mediumInnerName = child->property("inner_medium").getString();
        int mediumInner                   = -1;

        if (!mediumInnerName.empty()) {
            if (!ctx.Options.Scene.medium(mediumInnerName)) {
                IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown medium " << mediumInnerName << std::endl;
                continue;
            } else {
                const auto it = ctx.Options.Scene.media().find(mediumInnerName);
                mediumInner   = (int)std::distance(ctx.Options.Scene.media().begin(), it);
            }
        }

        const std::string mediumOuterName = child->property("outer_medium").getString();
        int mediumOuter                   = -1;

        if (!mediumOuterName.empty()) {
            if (!ctx.Options.Scene.medium(mediumOuterName)) {
                IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown medium " << mediumOuterName << std::endl;
                continue;
            } else {
                const auto it = ctx.Options.Scene.media().find(mediumOuterName);
                mediumOuter   = (int)std::distance(ctx.Options.Scene.media().begin(), it);
            }
        }

        if (ctx.Lights->isAreaLight(pair.first)) {
            material_groups.emplace_back().emplace_back(pair);
            ctx.Materials.push_back(Material{ bsdfName, mediumInner, mediumOuter, pair.first, 1 });
        } else {
            Material mat{ bsdfName, mediumInner, mediumOuter, {}, 1 };
            auto it = std::find(ctx.Materials.begin(), ctx.Materials.end(), mat);
            if (it == ctx.Materials.end()) {
                material_groups.emplace_back().emplace_back(pair);
                ctx.Materials.push_back(mat);
            } else {
                it->Count += 1;
                const auto id = std::distance(ctx.Materials.begin(), it);
                material_groups.at(id).emplace_back(pair);
            }
        }
    }

    // Load entities in the order of their material
    std::unordered_map<ShapeProvider*, std::vector<EntityObject>> in_objs;
    mEntityCount = 0;
    for (size_t materialID = 0; materialID < material_groups.size(); ++materialID) {
        for (const auto& pair : material_groups.at(materialID)) {
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
            ctx.SceneBBox.extend(entityBox);

            // Make sure the entity is added to the emissive list if it is associated with an area light
            if (ctx.Lights->isAreaLight(pair.first))
                ctx.EmissiveEntities.insert({ pair.first, Entity{ mEntityCount, transform, pair.first, shapeID, ctx.Materials.at(materialID).BSDF } });

            const Eigen::Matrix<float, 3, 4> toLocal        = invTransform.matrix().block<3, 4>(0, 0);
            const Eigen::Matrix<float, 3, 4> toGlobal       = transform.matrix().block<3, 4>(0, 0);
            const Eigen::Matrix<float, 3, 3> toGlobalNormal = toGlobal.block<3, 3>(0, 0).inverse().transpose();

            // Write data to dyntable
            auto& entityData = entityTable.addEntry(0);
            VectorSerializer entitySerializer(entityData, false);
            entitySerializer.write(toLocal, true);        // +12 = 12, To Local
            entitySerializer.write(toGlobal, true);       // +12 = 24, To Global
            entitySerializer.write(toGlobalNormal, true); // +9  = 33, To Global [Normal]
            entitySerializer.write((uint32)shapeID);      // +1  = 34
            entitySerializer.write((uint32)0);            // +1  = 35, Padding
            entitySerializer.write((uint32)0);            // +1  = 36, Padding

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
    }

    IG_LOG(L_DEBUG) << "Storing Entities took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

    ctx.EntityCount = mEntityCount;
    if (ctx.EntityCount == 0) {
        ctx.SceneDiameter = 0;
        return true;
    }

    ctx.SceneDiameter = ctx.SceneBBox.diameter().norm();

    // Build bvh (keep in mind that this BVH has no pre-padding as in the case for shape BVHs)
    IG_LOG(L_DEBUG) << "Generating BVH for scene" << std::endl;
    const auto start2 = std::chrono::high_resolution_clock::now();
    for (auto& p : in_objs) {
        auto& bvh = ctx.Database.SceneBVHs[p.first->identifier()];
        if (ctx.Options.Target.isGPU()) {
            setup_bvh<2>(p.second, bvh);
        } else if (ctx.Options.Target.vectorWidth() < 8) {
            setup_bvh<4>(p.second, bvh);
        } else {
            setup_bvh<8>(p.second, bvh);
        }
    }
    IG_LOG(L_DEBUG) << "Building Scene BVH took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start2).count() / 1000.0f << " seconds" << std::endl;

    return true;
}

} // namespace IG