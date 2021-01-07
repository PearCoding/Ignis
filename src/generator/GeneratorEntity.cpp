#include "GeneratorEntity.h"

#include "Logger.h"

#include "BVHIO.h"
#include "SceneBVHAdapter.h"

namespace IG {

using namespace Loader;
void GeneratorEntity::setup(GeneratorContext& ctx)
{
	// Fill entity list
	for (const auto& pair : ctx.Scene.entities()) {
		const auto child = pair.second;

		const std::string shapeName = child->property("shape").getString();
		if (ctx.Environment.Shapes.count(shapeName) == 0) {
			IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown shape " << shapeName << std::endl;
			continue;
		}

		const std::string bsdfName = child->property("bsdf").getString();
		if (!bsdfName.empty() && !ctx.Scene.bsdf(bsdfName)) {
			IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown bsdf " << bsdfName << std::endl;
			continue;
		}

		ctx.Environment.Entities.insert({ pair.first, Entity{ child->property("transform").getTransform(), shapeName, bsdfName } });
	}

	// Build bounding box
	ctx.Environment.SceneBBox = BoundingBox::Empty();
	std::vector<BoundingBox> boundingBoxes;
	boundingBoxes.reserve(ctx.Environment.Entities.size());

	for (const auto& pair : ctx.Environment.Entities) {
		const BoundingBox& shapeBox	= ctx.Environment.Shapes[pair.second.Shape].BoundingBox;
		const BoundingBox entityBox = shapeBox.transformed(pair.second.Transform);

		boundingBoxes.emplace_back(entityBox);
		ctx.Environment.SceneBBox.extend(entityBox);
	}
	
	ctx.Environment.SceneDiameter = (ctx.Environment.SceneBBox.max - ctx.Environment.SceneBBox.min).norm();

	// Build bvh
	if (ctx.ForceBVHBuild || IO::must_build_bvh("", ctx.FilePath.string(), ctx.Target)) {
		IG_LOG(L_INFO) << "Generating BVH for scene" << std::endl;
		IO::remove_bvh("");
		if (ctx.Target == Target::NVVM_STREAMING || ctx.Target == Target::NVVM_MEGAKERNEL || ctx.Target == Target::AMDGPU_STREAMING || ctx.Target == Target::AMDGPU_MEGAKERNEL) {
			std::vector<typename BvhNBbox<2>::Node> nodes;
			std::vector<TaggedBBox> objs;
			build_scene_bvh<2>(nodes, objs, boundingBoxes);
			IO::write_bvh("", nodes, objs);
		} else if (ctx.Target == Target::GENERIC || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
			std::vector<typename BvhNBbox<4>::Node> nodes;
			std::vector<TaggedBBox> objs;
			build_scene_bvh<4>(nodes, objs, boundingBoxes);
			IO::write_bvh("", nodes, objs);
		} else {
			std::vector<typename BvhNBbox<8>::Node> nodes;
			std::vector<TaggedBBox> objs;
			build_scene_bvh<8>(nodes, objs, boundingBoxes);
			IO::write_bvh("", nodes, objs);
		}
		IO::write_bvh_stamp("", ctx.FilePath.string(), ctx.Target);
	} else {
		IG_LOG(L_INFO) << "Reusing existing BVH for scene" << std::endl;
	}
}

} // namespace IG