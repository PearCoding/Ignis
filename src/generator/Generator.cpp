#include "Generator.h"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "BVHAdapter.h"
#include "IO.h"

#include "Logger.h"

#include "GeneratorBSDF.h"
#include "GeneratorLight.h"
#include "GeneratorShape.h"

namespace IG {
/* Notice: Ignis only supports a small subset of the Mitsuba (0.6 and 2.0) project files */

using namespace TPM_NAMESPACE;

struct SceneBuilder {
	GeneratorContext Context;

	inline void setup_integrator(const Object& obj, std::ostream& os)
	{
		// TODO: Use the sensor sample count as ssp
		size_t lastMPL = Context.MaxPathLen;
		for (const auto& child : obj.anonymousChildren()) {
			if (child->type() != OT_INTEGRATOR)
				continue;

			lastMPL = child->property("max_depth").getInteger(Context.MaxPathLen);
			if (child->pluginType() == "path") {
				os << "    let renderer = make_path_tracing_renderer(" << lastMPL << " /*max_path_len*/, " << Context.SPP << " /*spp*/);\n";
				return;
			} else if (child->pluginType() == "debug") {
				os << "     let renderer = make_debug_renderer();\n";
				return;
			}
		}

		IG_LOG(L_WARNING) << "No known integrator specified, therefore using path tracer" << std::endl;
		os << "    let renderer = make_path_tracing_renderer(" << lastMPL << " /*max_path_len*/, " << Context.SPP << " /*spp*/);\n";
	}

	inline void setup_camera(const Object& /*obj*/, std::ostream& os)
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

	void setup_shapes(const Object& elem, std::ostream& os)
	{
		GeneratorShape::setup(elem, Context);

		if (Context.Environment.Shapes.empty()) {
			IG_LOG(L_ERROR) << "No mesh available" << std::endl;
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
		   << "    };\n"
		   << "    let bvh = device.load_bvh(\"data/bvh.bin\");\n";

		if (Context.Environment.Mesh.face_area.size() < 4) // Make sure it is not too small
			Context.Environment.Mesh.face_area.resize(16);
		IO::write_tri_mesh(Context.Environment.Mesh, Context.EnablePadding);

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

	void setup_bitmap_textures(const Object& elem, std::ostream& os)
	{
		if (Context.Environment.Textures.empty())
			return;

		std::unordered_set<std::string> mapped;

		IG_LOG(L_INFO) << "Generating images for " << Context.FilePath << "" << std::endl;
		os << "\n    // Images\n";
		for (const auto& tex : Context.Environment.Textures) {
			if (tex->pluginType() != "bitmap")
				continue;

			std::string filename = tex->property("filename").getString();
			if (filename.empty()) {
				IG_LOG(L_WARNING) << "Invalid texture found" << std::endl;
				continue;
			}

			if (mapped.count(filename)) // FIXME: Different properties?
				continue;

			mapped.insert(filename);
			os << "    " << Context.generateTextureLoadInstruction(filename) << "\n";
		}
	}

	void setup_materials(const Object& elem, std::ostream& os)
	{
		if (Context.Environment.Materials.empty())
			return;

		IG_LOG(L_INFO) << "Generating trimesh lights for " << Context.FilePath << "" << std::endl;
		os << "\n    // Emission\n";
		size_t light_counter = 0;
		for (const auto& mat : Context.Environment.Materials) {
			if (!mat.Light)
				continue;

			const auto& shape = Context.Environment.Shapes[mat.MeshLightPairId];
			os << "    let light_" << light_counter << " = make_trimesh_light(math, tri_mesh, "
			   << (shape.ItxOffset / 4) << ", " << (shape.ItxCount / 4) << ", "
			   << Context.extractMaterialPropertyColor(mat.Light, "radiance") << ");\n";
			++light_counter;
		}

		IG_LOG(L_INFO) << "Generating materials for " << Context.FilePath << "" << std::endl;
		os << "\n    // Materials\n";

		light_counter = 0;
		for (size_t i = 0; i < Context.Environment.Materials.size(); ++i) {
			const auto& mat = Context.Environment.Materials[i];
			os << "    let material_" << i << " : Shader = @ |ray, hit, surf| {\n"
			   << "        let bsdf = " << GeneratorBSDF::extract(mat.BSDF, Context) << ";\n";

			if (mat.Light)
				os << "        make_emissive_material(surf, bsdf, light_" << light_counter << ")\n";
			else
				os << "        make_material(bsdf)\n";
			os << "    };\n";

			if (mat.Light)
				++light_counter;
		}
	}

	size_t setup_lights(const Object& elem, std::ostream& os)
	{
		IG_LOG(L_INFO) << "Generating lights for " << Context.FilePath << "" << std::endl;
		size_t light_count = 0;
		// Make sure area lights are the first ones
		for (const auto& m : Context.Environment.Materials) {
			if (m.Light)
				++light_count;
		}

		for (const auto& child : elem.anonymousChildren()) {
			if (child->type() != OT_EMITTER)
				continue;
			os << "    let light_" << light_count << " = " << GeneratorLight::extract(child, Context) << ";\n";
			++light_count;
		}

		os << "\n    // Lights\n";
		if (light_count == 0) { // Camera light
			os << "    let lights = @ |_ : i32| make_camera_light(math, camera, white);\n";
		} else {
			os << "    let lights = @ |i : i32| match i {\n";
			for (size_t i = 0; i < light_count; ++i) {
				if (i == light_count - 1)
					os << "        _ => light_" << i << "\n";
				else
					os << "        " << i << " => light_" << i << ",\n";
			}
			os << "    };\n";
		}

		return light_count;
	}

	void convert_scene(const Scene& scene, std::ostream& os)
	{
		setup_integrator(scene, os);
		setup_camera(scene, os);
		setup_shapes(scene, os);
		setup_bitmap_textures(scene, os);
		setup_materials(scene, os);
		size_t light_count = setup_lights(scene, os);

		os << "\n    // Geometries\n"
		   << "    let geometries = @ |i : i32| match i {\n";
		for (uint32_t s = 0; s < Context.Environment.Materials.size(); ++s) {
			os << "        ";
			if (s != Context.Environment.Materials.size() - 1)
				os << s;
			else
				os << "_";
			os << " => make_tri_mesh_geometry(math, tri_mesh, material_" << s << "),\n";
		}
		os << "    };\n";

		os << "\n    // Scene\n"
		   << "    let scene = Scene {\n"
		   << "        num_geometries = " << Context.Environment.Materials.size() << ",\n"
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
	IG_LOG(L_INFO) << "Converting MTS file " << filepath << "" << std::endl;

	try {
		SceneLoader loader;
		loader.addArgument("SPP", std::to_string(options.spp));
		loader.addArgument("MAX_PATH_LENGTH", std::to_string(options.max_path_length));

		auto scene = loader.loadFromFile(filepath);

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

		builder.convert_scene(scene, os);

		os << "\n"
		   << "    renderer(scene, device, iter);\n"
		   << "    device.present();\n"
		   << "}\n";
	} catch (const std::exception& e) {
		IG_LOG(L_ERROR) << "Invalid MTS file: " << e.what() << std::endl;
		return false;
	}

	IG_LOG(L_INFO) << "Scene was converted successfully" << std::endl;
	return true;
}
} // namespace IG