#include "LoaderEntity.h"

#include "Logger.h"

#include "bvh/SceneBVHAdapter.h"

namespace IG {
template <int N>
using StMatXf = Eigen::Matrix<float, N, N, Eigen::ColMajor | Eigen::DontAlign>;

using namespace Parser;

void LoaderEntity::setup(LoaderContext& ctx)
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

		const std::string bsdfName = child->property("bsdf").getString();
		if (!bsdfName.empty() && !ctx.Scene.bsdf(bsdfName)) {
			IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown bsdf " << bsdfName << std::endl;
			continue;
		}

		ctx.Environment.Entities.push_back(Entity{ child->property("transform").getTransform(), shapeID, 0 /*TODO*/ });
	}

	if (ctx.Environment.Entities.empty()) {
		IG_LOG(L_ERROR) << "No entities in the scene!" << std::endl;
		return;
	}

	// Write transform cache
	IG_LOG(L_INFO) << "Generating entity transform cache" << std::endl;
	std::vector<StMatXf<4>> local(ctx.Environment.Entities.size());
	std::vector<StMatXf<4>> global(ctx.Environment.Entities.size());
	std::vector<StMatXf<3>> normal(ctx.Environment.Entities.size());
	size_t counter = 0;
	for (const auto& ent : ctx.Environment.Entities) {
		global[counter] = ent.Transform.matrix();
		local[counter]	= global[counter].inverse();
		normal[counter] = global[counter].transpose().inverse().block<3, 3>(0, 0);
		++counter;
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
	if (ctx.Target == Target::NVVM_STREAMING || ctx.Target == Target::NVVM_MEGAKERNEL || ctx.Target == Target::AMDGPU_STREAMING || ctx.Target == Target::AMDGPU_MEGAKERNEL) {
		std::vector<typename BvhNEnt<2>::Node> nodes;
		std::vector<EntityLeaf1> objs;
		build_scene_bvh<2>(nodes, objs, in_objs);
		// TODO
	} else if (ctx.Target == Target::GENERIC || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
		std::vector<typename BvhNEnt<4>::Node> nodes;
		std::vector<EntityLeaf1> objs;
		build_scene_bvh<4>(nodes, objs, in_objs);
		// TODO
	} else {
		std::vector<typename BvhNEnt<8>::Node> nodes;
		std::vector<EntityLeaf1> objs;
		build_scene_bvh<8>(nodes, objs, in_objs);
		// TODO
	}
}

std::string LoaderEntity::dump(const LoaderContext& ctx)
{
	std::stringstream sstream;

	return sstream.str();
}

} // namespace IG