#include "LoaderEntity.h"
#include "Loader.h"
#include "Logger.h"
#include "bvh/SceneBVHAdapter.h"
#include "serialization/VectorSerializer.h"

namespace IG {
using namespace Parser;

template <typename Derived>
inline void writeMatrix(Serializer& serializer, const Eigen::MatrixBase<Derived>& m)
{
	for (int i = 0; i < m.rows(); ++i)
		for (int j = 0; j < m.cols(); ++j)
			serializer.write(m(i, j));
}

bool LoaderEntity::load(LoaderContext& ctx, LoaderResult& result)
{
	// Fill entity list
	for (const auto& pair : ctx.Scene.entities()) {
		const auto child = pair.second;

		const std::string shapeName = child->property("shape").getString();
		if (ctx.Environment.ShapeIDs.count(shapeName) == 0) {
			IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown shape " << shapeName << std::endl;
			continue;
		}
		uint32 shapeID = ctx.Environment.ShapeIDs.at(shapeName);

		/*const std::string bsdfName = child->property("bsdf").getString();
		if (!bsdfName.empty() && !ctx.Scene.bsdf(bsdfName)) {
			IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown bsdf " << bsdfName << std::endl;
			continue;
		}*/

		ctx.Environment.Entities.push_back(Entity{ child->property("transform").getTransform(), shapeID, 0 /*TODO*/ });
	}

	if (ctx.Environment.Entities.empty()) {
		// TODO: Make this possible
		IG_LOG(L_ERROR) << "No entities in the scene!" << std::endl;
		return false;
	}

	// Write transform cache
	IG_LOG(L_INFO) << "Generating entity transform cache" << std::endl;
	for (const auto& ent : ctx.Environment.Entities) {
		auto& entityData = result.Database.EntityTable.addLookup(0); // We do not make use of the typeid
		VectorSerializer entitySerializer(entityData, false);
		entitySerializer.write((uint32)ent.Shape);
		entitySerializer.write((uint32)0 /*TODO*/);
		entitySerializer.write((float)0); // Padding
		entitySerializer.write((float)0); // Padding
		writeMatrix(entitySerializer, ent.Transform.matrix());
		writeMatrix(entitySerializer, ent.Transform.inverse().matrix());
		writeMatrix(entitySerializer, ent.Transform.matrix().transpose().inverse().block<3, 3>(0, 0));
		entitySerializer.write((float)0); // Padding
		entitySerializer.write((float)0); // Padding
		entitySerializer.write((float)0); // Padding
	}

	// Build bounding box
	ctx.Environment.SceneBBox = BoundingBox::Empty();
	std::vector<EntityObject> in_objs;
	in_objs.reserve(ctx.Environment.Entities.size());

	for (const auto& ent : ctx.Environment.Entities) {
		const BoundingBox& shapeBox = ctx.Environment.Shapes[ent.Shape].BoundingBox;
		const BoundingBox entityBox = shapeBox.transformed(ent.Transform);

		ctx.Environment.SceneBBox.extend(entityBox);

		EntityObject obj;
		obj.BBox	   = entityBox;
		obj.Local	   = ent.Transform.inverse().matrix();
		obj.ShapeID	   = ent.Shape;
		obj.MaterialID = 0;
		in_objs.emplace_back(obj);
	}

	ctx.Environment.SceneDiameter = ctx.Environment.SceneBBox.diameter().norm();

	// Build bvh
	IG_LOG(L_INFO) << "Generating BVH for scene" << std::endl;
	VectorSerializer bvhSerializer(result.Database.BVH, false);
	if (ctx.Target == Target::NVVM_STREAMING || ctx.Target == Target::NVVM_MEGAKERNEL || ctx.Target == Target::AMDGPU_STREAMING || ctx.Target == Target::AMDGPU_MEGAKERNEL) {
		std::vector<typename BvhNEnt<2>::Node> nodes;
		std::vector<EntityLeaf1> objs;
		build_scene_bvh<2>(nodes, objs, in_objs);
		bvhSerializer.write((uint32)nodes.size());
		bvhSerializer.write(nodes, true);
		bvhSerializer.write(objs, true);
	} else if (ctx.Target == Target::GENERIC || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
		std::vector<typename BvhNEnt<4>::Node> nodes;
		std::vector<EntityLeaf1> objs;
		build_scene_bvh<4>(nodes, objs, in_objs);
		bvhSerializer.write((uint32)nodes.size());
		bvhSerializer.write(nodes, true);
		bvhSerializer.write(objs, true);
	} else {
		std::vector<typename BvhNEnt<8>::Node> nodes;
		std::vector<EntityLeaf1> objs;
		build_scene_bvh<8>(nodes, objs, in_objs);
		bvhSerializer.write((uint32)nodes.size());
		bvhSerializer.write(nodes, true);
		bvhSerializer.write(objs, true);
	}

	return true;
}

} // namespace IG