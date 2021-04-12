#include "Generator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BVHIO.h"
#include "IO.h"
#include "TriBVHAdapter.h"

#include "Logger.h"

#include "GeneratorBSDF.h"
#include "GeneratorEntity.h"
#include "GeneratorLight.h"
#include "GeneratorShape.h"
#include "GeneratorTexture.h"

namespace IG {
using namespace Loader;

static inline std::string writeMatrix(const Matrix4f& m)
{
	if (m.isIdentity())
		return "mat4x4_identity()";

	std::stringstream sstream;
	sstream << "make_mat4x4(";
	for (size_t col = 0; col < 4; ++col) {
		sstream << "make_vec4(" << m.col(col)(0) << ", " << m.col(col)(1) << ", " << m.col(col)(2) << ", " << m.col(col)(3) << ")";
		if (col != 3)
			sstream << ",";
	}
	sstream << ")";
	return sstream.str();
}

static inline std::string writeMatrix(const Matrix3f& m)
{
	if (m.isIdentity())
		return "mat3x3_identity()";

	std::stringstream sstream;
	sstream << "make_mat3x3(";
	for (size_t col = 0; col < 3; ++col) {
		sstream << "make_vec3(" << m.col(col)(0) << ", " << m.col(col)(1) << ", " << m.col(col)(2) << ")";
		if (col != 2)
			sstream << ",";
	}
	sstream << ")";
	return sstream.str();
}

struct SceneBuilder {
	GeneratorContext Context;

	inline void setup_integrator(std::ostream& os)
	{
		const auto technique = Context.Scene.technique();

		os << "    let emitter = if settings.ray_count > 0 { make_list_emitter(settings.ray_count, device.load_rays(settings.ray_count, settings.rays)) } else { make_camera_emitter() };\n";
		if (technique && technique->pluginType() == "debug") {
			os << "    let renderer = make_debug_renderer(emitter);\n";
		} else {
			size_t lastMPL = technique->property("max_depth").getInteger(Context.MaxPathLen);
			os << "    let renderer = make_path_tracing_renderer(" << lastMPL << " /*max_path_len*/, if settings.ray_count > 0 { 1 } else {" << Context.SPP << "} /*spp*/, emitter);\n";
			return;
		}
	}

	inline void setup_camera(std::ostream& os)
	{
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
		if (Context.Environment.Shapes.empty()) {
			IG_LOG(L_ERROR) << "No shapes available" << std::endl;
			return;
		}

		IG_LOG(L_INFO) << "Generating shapes for " << Context.FilePath << "" << std::endl;

		os << "\n    // Shapes\n";
		os << GeneratorShape::dump(Context);

		os << "\n    // Shape Mappings\n";
		os << "    let shapes = @ |i : i32| match i {\n";
		size_t counter = 0;
		for (const auto& pair : Context.Environment.Shapes) {
			if (counter == Context.Environment.Shapes.size() - 1)
				os << "        _ => ";
			else
				os << "        " << counter << " => ";

			os << "make_trimesh_shape(math, " << pair.second.TriMesh << ", " << pair.second.BVH << "),\n";
			++counter;
		}
		os << "    };\n";
	}

	void setup_entities(std::ostream& os)
	{
		if (Context.Environment.Entities.empty()) {
			IG_LOG(L_ERROR) << "No entities available" << std::endl;
			return;
		}

		IG_LOG(L_INFO) << "Generating entity mappings for " << Context.FilePath << "" << std::endl;
		os << "\n    // Entities\n";
		os << GeneratorEntity::dump(Context);
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
			os << "    let tex_" << GeneratorContext::makeId(name) << " = " << GeneratorTexture::extract(tex, Context) << ";\n";
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
			std::string line	   = GeneratorBSDF::prepare(mat, Context);
			if (!line.empty())
				os << line << "\n";
		}

		for (const auto& pair : Context.Scene.bsdfs()) {
			const std::string name = pair.first;
			const auto& mat		   = pair.second;
			os << "    let bsdf_" << GeneratorContext::makeId(name) << " = @ |ray : Ray, hit : Hit, surf : SurfaceElement| -> Bsdf { " << GeneratorBSDF::extract(mat, Context) << " };\n";
		}
	}

	void setup_lights(std::ostream& os)
	{
		IG_LOG(L_INFO) << "Generating lights for " << Context.FilePath << "" << std::endl;

		os << "\n    // Lights\n";
		for (const auto& pair : Context.Scene.lights()) {
			const auto name	 = pair.first;
			const auto light = pair.second;

			if (light->pluginType() == "area") {
				const std::string entityName = light->property("entity").getString();

				if (Context.Environment.Entities.count(entityName) > 0) {
					const auto& entity = Context.Environment.Entities[entityName];
					const Shape shape  = Context.Environment.Shapes[entity.Shape];

					const Matrix4f global_m = entity.Transform.matrix();
					const Matrix3f normal_m = global_m.transpose().inverse().block<3, 3>(0, 0);

					os << "    let light_" << GeneratorContext::makeId(name) << " = make_trimesh_light(math, "
					   << shape.TriMesh << ", 0, "
					   << (shape.ItxCount / 4) << ", "
					   << Context.extractMaterialPropertyColor(light, "radiance") << ", "
					   << writeMatrix(global_m) << ", "
					   << writeMatrix(normal_m) << ");\n";
				} else {
					IG_LOG(L_ERROR) << "Light " << name << " connected to unknown entity " << entityName << std::endl;
				}
			} else if (light->pluginType() == "sky" || light->pluginType() == "sunsky") {
				const std::string tex_name = GeneratorContext::makeId(name);
				os << "    let skytex_" << tex_name << " = make_texture(math, make_repeat_border(), make_bilinear_filter(), device.load_image(\"data/skytex_" << tex_name << ".exr\"));\n";
				os << "    let light_" << tex_name << " = " << GeneratorLight::extract(name, light, Context) << ";\n";
			} else {
				os << "    let light_" << GeneratorContext::makeId(name) << " = " << GeneratorLight::extract(name, light, Context) << ";\n";
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
					os << "        _ => light_" << GeneratorContext::makeId(pair.first) << "\n";
				else
					os << "        " << counter << " => light_" << GeneratorContext::makeId(pair.first) << ",\n";
				++counter;
			}
			os << "    };\n";
		}
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
			os << "    let material_" << GeneratorContext::makeId(pair.first) << " : Shader = @ |ray, hit, surf| ";
			if (isEmissive)
				os << "make_emissive_material(surf,";
			else
				os << "make_material(";

			if (pair.second.BSDF.empty())
				os << "make_black_bsdf()";
			else
				os << "bsdf_" << GeneratorContext::makeId(pair.second.BSDF) << "(ray, hit, surf)";

			if (isEmissive)
				os << ", light_" << GeneratorContext::makeId(entity_light_map[pair.first]);

			os << ");\n";
		}
	}

	void convert_scene(std::ostream& os)
	{
		GeneratorShape::setup(Context);
		GeneratorEntity::setup(Context);

		setup_integrator(os);
		setup_camera(os);
		setup_shapes(os);
		setup_textures(os);
		setup_bsdfs(os);
		setup_lights(os);
		setup_materials(os);
		setup_entities(os);

		os << "\n    // Scene\n"
		   << "    let bvh = device.load_scene_bvh(\"data/bvh.bin\");\n"
		   << "    let scene = Scene {\n"
		   << "        num_entities = " << Context.Environment.Entities.size() << ",\n"
		   << "        num_shapes   = " << Context.Environment.Shapes.size() << ",\n"
		   << "        num_lights   = " << Context.Scene.lights().size() << ",\n"
		   << "        entities     = entity_map, \n"
		   << "        shapes       = shapes,\n"
		   << "        lights       = lights,\n"
		   << "        camera       = camera,\n"
		   << "        bvh          = bvh\n"
		   << "    };\n";
	}

	void setup_default_settings(std::ostream& os)
	{
		constexpr int DEFAULT_WIDTH	 = 800;
		constexpr int DEFAULT_HEIGHT = 600;
		constexpr float DEFAULT_FOV	 = 60;

		const auto camera = Context.Scene.camera();
		const auto film	  = Context.Scene.film();

		const int width	 = film ? film->property("size").getVector2(Vector2f(DEFAULT_WIDTH, DEFAULT_HEIGHT)).x() : DEFAULT_WIDTH;
		const int height = film ? film->property("size").getVector2(Vector2f(DEFAULT_WIDTH, DEFAULT_HEIGHT)).y() : DEFAULT_HEIGHT;

		Vector3f eye = Vector3f::Zero();
		Vector3f dir = Vector3f(0, 0, 1);
		Vector3f up	 = Vector3f(0, 1, 0);
		float fov	 = DEFAULT_FOV;

		if (camera) {
			const Transformf t = camera->property("transform").getTransform();
			eye				   = t * eye;
			dir				   = t.linear().col(2);
			up				   = t.linear().col(1);

			fov = camera->property("fov").getNumber(DEFAULT_FOV);
		}

		os << "    eye         = make_vec3(" << eye(0) << ", " << eye(1) << ", " << eye(2) << "),\n"
		   << "    dir         = make_vec3(" << dir(0) << ", " << dir(1) << ", " << dir(2) << "),\n"
		   << "    up          = make_vec3(" << up(0) << ", " << up(1) << ", " << up(2) << "),\n"
		   << "    fov         = " << fov << ",\n"
		   << "    film_width  = " << width << ",\n"
		   << "    film_height = " << height << "\n";
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
		std::filesystem::create_directories("data/meshes");

		SceneBuilder builder;

		bool ok;
		builder.Context.Scene = loader.loadFromFile(filepath, ok);
		if (!ok) {
			IG_LOG(L_ERROR) << "Could not load JSON." << std::endl;
			return false;
		}

		builder.Context.FilePath	  = std::filesystem::canonical(filepath);
		builder.Context.Target		  = options.target;
		builder.Context.MaxPathLen	  = options.max_path_length;
		builder.Context.SPP			  = options.spp;
		builder.Context.Fusion		  = options.fusion;
		builder.Context.EnablePadding = require_padding(options.target);
		builder.Context.ForceBVHBuild = options.force_bvh_build;

		os << "//------------------------------------------------------------------------------------\n"
		   << "// Generated from " << filepath << " with ignis generator tool\n"
		   << "//------------------------------------------------------------------------------------\n"
		   << "\n"
		   << "struct InitialSettings {\n"
		   << "    eye:         Vec3,\n"
		   << "    dir:         Vec3,\n"
		   << "    up:          Vec3,\n"
		   << "    fov:         f32,\n"
		   << "    film_width:  i32,\n"
		   << "    film_height: i32\n"
		   << "}\n"
		   << "\n"
		   << "struct Settings {\n"
		   << "    eye:    Vec3,\n"
		   << "    dir:    Vec3,\n"
		   << "    up:     Vec3,\n"
		   << "    right:  Vec3,\n"
		   << "    width:  f32,\n"
		   << "    height: f32,\n"
		   << "    ray_count: i32,\n"
		   << "    rays: &[Ray]\n"
		   << "}\n"
		   << "\n"
		   << "#[export] fn get_spp() -> i32 { " << options.spp << " }\n"
		   << "\n";

		os << "#[export] fn get_default_settings() = InitialSettings {\n";
		builder.setup_default_settings(os);
		os << "};\n"
		   << "\n";

		os << "#[export] fn render(settings: &Settings, iter: i32) -> () {\n";

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