#include "Generator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BVHAdapter.h"
#include "IO.h"

#include "Logger.h"

#include "GeneratorBSDF.h"
#include "GeneratorEntity.h"
#include "GeneratorLight.h"
#include "GeneratorShape.h"
#include "GeneratorTexture.h"

namespace IG {
using namespace Loader;

struct SceneBuilder {
	GeneratorContext Context;

	inline static std::string makeId(const std::string& name)
	{
		std::string id = name; // TODO?
		std::transform(id.begin(), id.end(), id.begin(), [](char c) {
			if (std::isspace(c) || !std::isalnum(c))
				return '_';
			return c;
		});
		return id;
	}

	inline void setup_integrator(std::ostream& os)
	{
		const auto technique = Context.Scene.technique();

		// TODO: Use the sensor sample count as ssp
		size_t lastMPL = Context.MaxPathLen;
		if (technique) {
			lastMPL = technique->property("max_depth").getInteger(Context.MaxPathLen);
			if (technique->pluginType() == "path") {
				os << "    let renderer = make_path_tracing_renderer(" << lastMPL << " /*max_path_len*/, " << Context.SPP << " /*spp*/);\n";
				return;
			} else if (technique->pluginType() == "debug") {
				os << "     let renderer = make_debug_renderer();\n";
				return;
			}
		}

		IG_LOG(L_WARNING) << "No known integrator specified, therefore using path tracer" << std::endl;
		os << "    let renderer = make_path_tracing_renderer(" << lastMPL << " /*max_path_len*/, " << Context.SPP << " /*spp*/);\n";
	}

	inline void setup_camera(std::ostream& os)
	{
		// TODO: Extract default settings?
		// Setup camera
		os << "\n    // Camera\n"
		   << "    let camera = make_perspective_camera(\n"
		   << "        math,\n"
		   << "        settings.eye,\n"
		   << "        make_mat3x3(settings.right, settings.up, settings.dir),\n"
		   << "        settings.width,\n"
		   << "        settings.height\n"
		   << "    );\n";
	}

	void setup_shapes(std::ostream& os)
	{
		GeneratorShape::setup(Context);

		if (Context.Environment.Shapes.empty()) {
			IG_LOG(L_ERROR) << "No shapes available" << std::endl;
			return;
		}

		IG_LOG(L_INFO) << "Generating merged triangle mesh" << std::endl;
		os << "\n    // Triangle mesh\n"
		   << "    let vertices     = device.load_buffer(\"data/vertices.bin\");\n"
		   << "    let normals      = device.load_buffer(\"data/normals.bin\");\n"
		   << "    let face_normals = device.load_buffer(\"data/face_normals.bin\");\n"
		   << "    let face_area    = device.load_buffer(\"data/face_area.bin\");\n"
		   << "    let texcoords    = device.load_buffer(\"data/texcoords.bin\");\n"
		   << "    let indices      = device.load_buffer(\"data/indices.bin\");\n"
		   << "    let tri_mesh     = TriMesh {\n"
		   << "        vertices     = @ |i| vertices.load_vec3(i),\n"
		   << "        normals      = @ |i| normals.load_vec3(i),\n"
		   << "        face_normals = @ |i| face_normals.load_vec3(i),\n"
		   << "        face_area    = @ |i| face_area.load_f32(i),\n"
		   << "        triangles    = @ |i| { let (i0, i1, i2, _) = indices.load_int4(i); (i0, i1, i2) },\n"
		   << "        attrs        = @ |_| (false, @ |j| vec2_to_4(texcoords.load_vec2(j), 0.0, 0.0)),\n"
		   << "        num_attrs    = 1,\n"
		   << "        num_tris     = " << size_t(Context.Environment.Mesh.faceCount()) << "\n"
		   << "    };\n";

		if (Context.Environment.Mesh.face_area.size() < 4) // Make sure it is not too small
			Context.Environment.Mesh.face_area.resize(16);
		IO::write_tri_mesh(Context.Environment.Mesh, Context.EnablePadding);
	}

	void setup_entities(std::ostream& os)
	{
		GeneratorEntity::setup(Context);

		if (Context.Environment.Entities.empty()) {
			IG_LOG(L_ERROR) << "No entities available" << std::endl;
			return;
		}

		std::vector<uint32> mapping;
		mapping.reserve(Context.Environment.Entities.size());
		for (const auto& pair : Context.Environment.Entities) {
			uint32 shapeId = std::distance(Context.Scene.shapes().begin(), Context.Scene.shapes().find(pair.second.Shape));
			mapping.emplace_back(shapeId);
		}
		IO::write_buffer("data/entity_mappings.bin", mapping);

		std::vector<float> transforms;
		mapping.reserve(Context.Environment.Entities.size() * 16);
		for (const auto& pair : Context.Environment.Entities) {
			for (size_t i = 0; i < 16; ++i)
				transforms.emplace_back(pair.second.Transform.matrix()(i));
		}
		IO::write_buffer("data/entity_transforms.bin", IO::pad_buffer(transforms, Context.EnablePadding, sizeof(float) * 16));

		os << "\n    // Entity Mappings\n"
		   << "    let entity_mappings   = device.load_buffer(\"data/entity_mappings.bin\");\n"
		   << "    let entity_to_shape   = @ |id : i32| entity_mappings.load_i32(id);\n"
		   << "    let entity_transforms = device.load_buffer(\"data/entity_transforms.bin\");\n"
		   << "    let get_entity_mat    = @ |id : i32| make_mat4x4(entity_transforms.load_vec4(4*id),entity_transforms.load_vec4(4*id+1),entity_transforms.load_vec4(4*id+2),entity_transforms.load_vec4(4*id+3));\n"
		   << "    let bvh               = device.load_bvh(\"data/bvh.bin\");\n";

		// TODO
		IG_LOG(L_INFO) << "Generating entity table and BVH" << std::endl;
		// Generate BVHs
		if (IO::must_build_bvh(Context.FilePath.string(), Context.Target)) {
			IG_LOG(L_INFO) << "Generating BVH for " << Context.FilePath << "" << std::endl;
			std::remove("data/bvh.bin");
			if (Context.Target == Target::NVVM_STREAMING || Context.Target == Target::NVVM_MEGAKERNEL || Context.Target == Target::AMDGPU_STREAMING || Context.Target == Target::AMDGPU_MEGAKERNEL) {
				std::vector<typename BvhNTriM<2, 1>::Node> nodes;
				std::vector<typename BvhNTriM<2, 1>::Tri> tris;
				build_bvh<2, 1>(Context.Environment.Mesh, nodes, tris);
				IO::write_bvh(nodes, tris);
			} else if (Context.Target == Target::GENERIC || Context.Target == Target::ASIMD || Context.Target == Target::SSE42) {
				std::vector<typename BvhNTriM<4, 4>::Node> nodes;
				std::vector<typename BvhNTriM<4, 4>::Tri> tris;
				build_bvh<4, 4>(Context.Environment.Mesh, nodes, tris);
				IO::write_bvh(nodes, tris);
			} else {
				std::vector<typename BvhNTriM<8, 4>::Node> nodes;
				std::vector<typename BvhNTriM<8, 4>::Tri> tris;
				build_bvh<8, 4>(Context.Environment.Mesh, nodes, tris);
				IO::write_bvh(nodes, tris);
			}
			IO::write_bvh_stamp(Context.FilePath.string(), Context.Target);
		} else {
			IG_LOG(L_INFO) << "Reusing existing BVH for " << Context.FilePath << "" << std::endl;
		}

		// Calculate scene bounding box
		Context.Environment.calculateBoundingBox();
	}

	void setup_textures(std::ostream& os)
	{
		if (Context.Scene.textures().empty())
			return;

		IG_LOG(L_INFO) << "Generating textures for " << Context.FilePath << "" << std::endl;
		os << "\n    // Textures\n";
		for (const auto& pair : Context.Scene.textures()) {
			const std::string name = pair.first;
			const auto tex		   = pair.second;
			os << "    tex_" << makeId(name) << " = " << GeneratorTexture::extract(tex, Context) << ";\n";
		}
	}

	void setup_bsdfs(std::ostream& os)
	{
		if (Context.Scene.bsdfs().empty())
			return;

		IG_LOG(L_INFO) << "Generating bsdfs for " << Context.FilePath << "" << std::endl;
		os << "\n    // BSDFs\n";

		for (const auto& pair : Context.Scene.bsdfs()) {
			const std::string name = pair.first;
			const auto& mat		   = pair.second;
			os << "    let bsdf_" << makeId(name) << " = @ |ray : Ray, hit : Hit, surf : SurfaceElement| -> Bsdf { " << GeneratorBSDF::extract(mat, Context) << " };\n";
		}
	}

	size_t setup_lights(std::ostream& os)
	{
		IG_LOG(L_INFO) << "Generating lights for " << Context.FilePath << "" << std::endl;

		os << "\n    // Lights\n";
		for (const auto& pair : Context.Scene.lights()) {
			const auto name	 = pair.first;
			const auto light = pair.second;

			if (light->pluginType() != "area") {
				os << "    let light_" << makeId(name) << " = " << GeneratorLight::extract(light, Context) << ";\n";
			} else {
				const std::string entityName = light->property("entity").getString();

				if (Context.Environment.Entities.count(entityName) > 0) {
					const auto& entity = Context.Environment.Entities[entityName];
					const Shape shape  = Context.Environment.Shapes[entity.Shape];
					os << "    let light_" << makeId(name) << " = make_trimesh_light(math, tri_mesh, "
					   << (shape.ItxOffset / 4) << ", " << (shape.ItxCount / 4) << ", "
					   << Context.extractMaterialPropertyColor(light, "radiance") << ");\n";
				} else {
					IG_LOG(L_ERROR) << "Light " << name << " connected to unknown entity " << entityName << std::endl;
				}
			}
		}

		os << "\n    // Light Mappings\n";
		if (Context.Scene.lights().empty()) { // Camera light
			os << "    let lights = @ |_ : i32| make_camera_light(math, camera, white);\n";
		} else {
			os << "    let lights = @ |i : i32| match i {\n";
			size_t counter = 0;
			for (const auto& pair : Context.Scene.lights()) {
				if (counter == Context.Scene.lights().size() - 1)
					os << "        _ => light_" << makeId(pair.first) << "\n";
				else
					os << "        " << counter << " => light_" << makeId(pair.first) << ",\n";
				++counter;
			}
			os << "    };\n";
		}

		return Context.Scene.lights().size();
	}

	void setup_materials(std::ostream& os)
	{
		IG_LOG(L_INFO) << "Generating materials for " << Context.FilePath << "" << std::endl;

		// Extract area light entity connections
		std::unordered_map<std::string, std::string> entity_light_map;
		for (const auto& pair : Context.Scene.lights()) {
			const auto name	 = pair.first;
			const auto light = pair.second;

			if (light->pluginType() != "area")
				continue;

			const std::string entityName = light->property("entity").getString();
			if (entity_light_map.count(entityName)) {
				IG_LOG(L_ERROR) << "Entity " << entityName << " is already defined as light!" << std::endl;
				continue;
			}

			entity_light_map.insert({ entityName, name });
		}

		// Setup "materials"
		os << "\n    // Materials\n";
		for (const auto& pair : Context.Environment.Entities) {
			const bool isEmissive = entity_light_map.count(pair.first);
			os << "    let material_" << makeId(pair.first) << " : Shader = @ |ray, hit, surf| ";
			if (isEmissive)
				os << "make_emissive_material(surf,";
			else
				os << "make_material(";

			if (pair.second.BSDF.empty())
				os << "make_black_bsdf()";
			else
				os << "bsdf_" << makeId(pair.second.BSDF) << "(ray, hit, surf)";

			if (isEmissive)
				os << ", light_" << makeId(entity_light_map[pair.first]);

			os << ");\n";
		}
	}

	void setup_geometries(std::ostream& os)
	{
		IG_LOG(L_INFO) << "Generating geometries for " << Context.FilePath << "" << std::endl;

		// Setup geometries
		os << "\n    // Geometries\n"
		   << "    let geometries = @ |i : i32| match i {\n";
		if (Context.Environment.Entities.empty()) {
			os << "        _ =>  make_tri_mesh_geometry(math, tri_mesh, @ |_, _, _| make_empty_material())\n";
		} else {
			size_t counter = 0;
			for (const auto& pair : Context.Environment.Entities) {
				os << "        ";
				if (counter != Context.Environment.Entities.size() - 1)
					os << counter;
				else
					os << "_";
				os << " => make_tri_mesh_geometry(math, tri_mesh, material_" << makeId(pair.first) << "),\n";
				++counter;
			}
		}
		os << "    };\n";
	}

	void convert_scene(std::ostream& os)
	{
		setup_integrator(os);
		setup_camera(os);
		setup_shapes(os);
		setup_entities(os);
		setup_textures(os);
		setup_bsdfs(os);
		size_t light_count = setup_lights(os);
		setup_materials(os);
		setup_geometries(os);

		os << "\n    // Scene\n"
		   << "    let scene = Scene {\n"
		   << "        num_geometries = " << Context.Environment.Entities.size() << ",\n"
		   << "        num_lights     = " << light_count << ",\n"
		   << "        geometries     = @ |i : i32| geometries(i),\n"
		   << "        lights         = @ |i : i32| lights(i),\n"
		   << "        camera         = camera,\n"
		   << "        bvh            = bvh\n"
		   << "    };\n";
	}
};

bool generate(const std::filesystem::path& filepath, const GeneratorOptions& options, std::ostream& os)
{
	IG_LOG(L_INFO) << "Converting JSON file " << filepath << "" << std::endl;

	try {
		SceneLoader loader;
		loader.addArgument("SPP", std::to_string(options.spp));
		loader.addArgument("MAX_PATH_LENGTH", std::to_string(options.max_path_length));

		std::filesystem::create_directories("data/textures");

		os << "//------------------------------------------------------------------------------------\n"
		   << "// Generated from " << filepath << " with ignis generator tool\n"
		   << "//------------------------------------------------------------------------------------\n"
		   << "\n"
		   << "struct Settings {\n"
		   << "    eye: Vec3,\n"
		   << "    dir: Vec3,\n"
		   << "    up: Vec3,\n"
		   << "    right: Vec3,\n"
		   << "    width: f32,\n"
		   << "    height: f32\n"
		   << "}\n"
		   << "\n"
		   << "#[export] fn get_spp() -> i32 { " << options.spp << " }\n"
		   << "\n"
		   << "#[export] fn render(settings: &Settings, iter: i32) -> () {\n";

		SceneBuilder builder;
		builder.Context.Scene		  = loader.loadFromFile(filepath);
		builder.Context.FilePath	  = std::filesystem::canonical(filepath);
		builder.Context.Target		  = options.target;
		builder.Context.MaxPathLen	  = options.max_path_length;
		builder.Context.SPP			  = options.spp;
		builder.Context.Fusion		  = options.fusion;
		builder.Context.EnablePadding = require_padding(options.target);

		switch (options.target) {
		case Target::GENERIC:
			os << "    let device   = make_cpu_default_device();\n";
			break;
		case Target::AVX2:
			os << "    let device   = make_avx2_device();\n";
			break;
		case Target::AVX:
			os << "    let device   = make_avx_device();\n";
			break;
		case Target::SSE42:
			os << "    let device   = make_sse42_device();\n";
			break;
		case Target::ASIMD:
			os << "    let device   = make_asimd_device();\n";
			break;
		case Target::NVVM_STREAMING:
			os << "    let device   = make_nvvm_device(" << options.device << ", true);\n";
			break;
		case Target::NVVM_MEGAKERNEL:
			os << "    let device   = make_nvvm_device(" << options.device << ", false);\n";
			break;
		case Target::AMDGPU_STREAMING:
			os << "    let device   = make_amdgpu_device(" << options.device << ", true);\n";
			break;
		case Target::AMDGPU_MEGAKERNEL:
			os << "    let device   = make_amdgpu_device(" << options.device << ", false);\n";
			break;
		default:
			assert(false);
			break;
		}

		os << "    let math     = device.intrinsics;\n";

		builder.convert_scene(os);

		os << "\n"
		   << "    renderer(scene, device, iter);\n"
		   << "    device.present();\n"
		   << "}\n";
	} catch (const std::exception& e) {
		IG_LOG(L_ERROR) << "Invalid JSON file: " << e.what() << std::endl;
		return false;
	}

	IG_LOG(L_INFO) << "Scene was converted successfully" << std::endl;
	return true;
}
} // namespace IG