#include "GeneratorEntity.h"

#include "Logger.h"

#include "BVHIO.h"
#include "IO.h"
#include "SceneBVHAdapter.h"

namespace IG {
template <int N>
using StMatXf = Eigen::Matrix<float, N, N, Eigen::ColMajor | Eigen::DontAlign>;

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
	for (const auto& pair : ctx.Environment.Entities) {
		global[counter]	   = pair.second.Transform.matrix();
		local[counter]	   = global[counter].inverse();
		normal[counter]	   = global[counter].transpose().inverse().block<3, 3>(0, 0);
		++counter;
	}

	IO::write_buffer("data/entity_t_local.bin", IO::pad_buffer(local, ctx.EnablePadding, sizeof(float) * 16));
	IO::write_buffer("data/entity_t_global.bin", IO::pad_buffer(global, ctx.EnablePadding, sizeof(float) * 16));
	IO::write_buffer("data/entity_t_normal.bin", IO::pad_buffer(normal, ctx.EnablePadding, sizeof(float) * 12));

	// Build bounding box
	ctx.Environment.SceneBBox = BoundingBox::Empty();
	std::vector<BoundingBox> boundingBoxes;
	boundingBoxes.reserve(ctx.Environment.Entities.size());

	for (const auto& pair : ctx.Environment.Entities) {
		const BoundingBox& shapeBox = ctx.Environment.Shapes[pair.second.Shape].BoundingBox;
		const BoundingBox entityBox = shapeBox.transformed(pair.second.Transform);

		boundingBoxes.emplace_back(entityBox);
		ctx.Environment.SceneBBox.extend(entityBox);
	}

	ctx.Environment.SceneDiameter = ctx.Environment.SceneBBox.diameter().norm();

	// Build bvh
	if (ctx.ForceBVHBuild || IO::must_build_bvh("", ctx.FilePath.string(), ctx.Target)) {
		IG_LOG(L_INFO) << "Generating BVH for scene" << std::endl;
		IO::remove_bvh("");
		if (ctx.Target == Target::NVVM_STREAMING || ctx.Target == Target::NVVM_MEGAKERNEL || ctx.Target == Target::AMDGPU_STREAMING || ctx.Target == Target::AMDGPU_MEGAKERNEL) {
			std::vector<typename BvhNBbox<2>::Node> nodes;
			std::vector<EntityLeaf1> objs;
			build_scene_bvh<2>(nodes, objs, boundingBoxes);
			IO::write_bvh("", nodes, objs);
		} else if (ctx.Target == Target::GENERIC || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
			std::vector<typename BvhNBbox<4>::Node> nodes;
			std::vector<EntityLeaf1> objs;
			build_scene_bvh<4>(nodes, objs, boundingBoxes);
			IO::write_bvh("", nodes, objs);
		} else {
			std::vector<typename BvhNBbox<8>::Node> nodes;
			std::vector<EntityLeaf1> objs;
			build_scene_bvh<8>(nodes, objs, boundingBoxes);
			IO::write_bvh("", nodes, objs);
		}
		IO::write_bvh_stamp("", ctx.FilePath.string(), ctx.Target);
	} else {
		IG_LOG(L_INFO) << "Reusing existing BVH for scene" << std::endl;
	}
}

std::string GeneratorEntity::dump(const GeneratorContext& ctx)
{
	std::stringstream sstream;

	sstream << "    let entity_local  = device.load_buffer(\"data/entity_t_local.bin\");\n"
			<< "    let entity_global = device.load_buffer(\"data/entity_t_global.bin\");\n"
			<< "    let entity_normal = device.load_buffer(\"data/entity_t_normal.bin\");\n";

	sstream << "    let entity_shaders = @ |i : i32| match i {\n";
	size_t counter = 0;
	for (const auto& pair : ctx.Environment.Entities) {
		const std::string id = GeneratorContext::makeId(pair.first);

		if (counter == ctx.Environment.Entities.size() - 1)
			sstream << "        _ => ";
		else
			sstream << "        " << counter << " => ";

		sstream << " material_" << id << ",\n";
		++counter;
	}
	sstream << "    };\n";

	sstream << "    let entity_shapes = @ |i : i32| match i {\n";
	counter = 0;
	for (const auto& pair : ctx.Environment.Entities) {
		const std::string id = GeneratorContext::makeId(pair.first);
		const uint32 shapeID = std::distance(ctx.Environment.Shapes.begin(), ctx.Environment.Shapes.find(pair.second.Shape));

		if (counter == ctx.Environment.Entities.size() - 1)
			sstream << "        _ => ";
		else
			sstream << "        " << counter << " => ";

		sstream << shapeID << ",\n";
		++counter;
	}
	sstream << "    };\n";

	sstream << "    let entity_map = EntityMap {\n"
			<< "       local_mat4x4  = @ |i| entity_local.load_mat4x4(i),\n"
			<< "       global_mat4x4 = @ |i| entity_global.load_mat4x4(i),\n"
			<< "       normal_mat3x3 = @ |i| entity_normal.load_mat3x3(i),\n"
			<< "       shape         = entity_shapes,\n"
			<< "       shader        = entity_shaders,\n"
			<< "    };\n";

	return sstream.str();
}

} // namespace IG