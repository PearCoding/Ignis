#include "Generator.h"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "BVHAdapter.h"
#include "IO.h"

#include "Logger.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

#include "tinyparser-mitsuba.h"

namespace IG {
/* Notice: Ignis only supports a small subset of the Mitsuba (0.6 and 2.0) project files */

using namespace TPM_NAMESPACE;

inline std::string escape_f32(float f)
{
	if (std::isinf(f) && !std::signbit(f)) {
		return "flt_inf";
	} else if (std::isinf(f) && std::signbit(f)) {
		return "-flt_inf";
	} else {
		std::stringstream sstream;
		sstream << f;
		return sstream.str();
	}
}

struct LoadInfo {
	std::filesystem::path FilePath;
	IG::Target Target;
	size_t MaxPathLen;
	size_t SPP;
	bool Fusion;
	bool EnablePadding;

	inline std::filesystem::path handlePath(const std::filesystem::path& path) const
	{
		if (path.is_absolute())
			return path;
		else {
			const auto p = std::filesystem::canonical(FilePath.parent_path() / path);
			if (std::filesystem::exists(p))
				return p;
			else
				return std::filesystem::canonical(path);
		}
	}

	inline std::string makeId(const std::filesystem::path& path) const
	{
		std::string id = path; // TODO?
		std::transform(id.begin(), id.end(), id.begin(), [](char c) {
			if (std::isspace(c) || !std::isalnum(c))
				return '_';
			return c;
		});
		return id;
	}
};

struct Material {
	uint32_t MeshId = 0;
	std::shared_ptr<Object> BSDF;
	std::shared_ptr<Object> Light;
};

inline bool operator==(const Material& a, const Material& b)
{
	return a.MeshId == b.MeshId && a.BSDF == b.BSDF && a.Light == b.Light;
}

class MaterialHash {
public:
	size_t operator()(const Material& s) const
	{
		size_t h1 = std::hash<decltype(s.BSDF)>()(s.BSDF);
		size_t h2 = std::hash<decltype(s.Light)>()(s.Light);
		size_t h3 = std::hash<decltype(s.MeshId)>()(s.MeshId);
		return (h1 ^ (h2 << 1)) ^ (h3 << 1);
	}
};

struct Shape {
	size_t VtxOffset;
	size_t ItxOffset;
	size_t VtxCount;
	size_t ItxCount;
	IG::Material Material;
};

struct GenContext {
	std::vector<Shape> Shapes;
	std::vector<Material> Materials;
	std::unordered_set<std::shared_ptr<Object>> Textures;
	TriMesh Mesh;
	BoundingBox SceneBBox;
	float SceneDiameter = 0.0f;
};

inline bool is_simple_brdf(const std::string& brdf)
{
	return brdf == "diffuse";
}

inline void setup_integrator(const Object& obj, const LoadInfo& info, std::ostream& os)
{
	// TODO: Use the sensor sample count as ssp
	size_t lastMPL = info.MaxPathLen;
	for (const auto& child : obj.anonymousChildren()) {
		if (child->type() != OT_INTEGRATOR)
			continue;

		lastMPL = child->property("max_depth").getInteger(info.MaxPathLen);
		if (child->pluginType() == "path") {
			os << "    let renderer = make_path_tracing_renderer(" << lastMPL << " /*max_path_len*/, " << info.SPP << " /*spp*/);\n";
			return;
		}
	}

	IG_LOG(L_WARNING) << "No known integrator specified, therefore using path tracer" << std::endl;
	os << "    let renderer = make_path_tracing_renderer(" << lastMPL << " /*max_path_len*/, " << info.SPP << " /*spp*/);\n";
	//os << "     let renderer = make_debug_renderer();\n";
}

inline void setup_camera(const Object& obj, const LoadInfo& info, std::ostream& os)
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

inline Vector3f applyRotationScale(const Transform& t, const Vector3f& v)
{
	Vector3f res;
	for (int i = 0; i < 3; ++i) {
		res[i] = 0;
		for (int j = 0; j < 3; ++j)
			res[i] += t(i, j) * v[j];
	}
	return res;
}

inline Vector3f applyTransformAffine(const Transform& t, const Vector3f& v)
{
	return applyRotationScale(t, v) + Vector3f(t(0, 3), t(1, 3), t(2, 3));
}

// Apply inverse of transpose of orthogonal part of the transform
// which is the original orthogonal part if non-uniform scale is prohibited.
// TODO: We are ignoring non-uniform scale properties
inline Vector3f applyNormalTransform(const Transform& t, const Vector3f& v)
{
	return applyRotationScale(t, v);
}

// Unpack bsdf such that twosided materials are ignored and texture nodes are registered
inline std::shared_ptr<Object> add_bsdf(const std::shared_ptr<Object>& elem, GenContext& ctx)
{
	if (elem->pluginType() == "twosided") {
		if (elem->anonymousChildren().size() != 1)
			IG_LOG(L_ERROR) << "Invalid twosided bsdf" << std::endl;
		// IG_LOG(L_WARNING) << "Ignoring twosided bsdf" << std::endl;
		return add_bsdf(elem->anonymousChildren().front(), ctx);
	} else {
		for (const auto& child : elem->namedChildren()) {
			if (child.second->type() == OT_TEXTURE)
				ctx.Textures.insert(child.second);
		}

		return elem;
	}
}

// Unpack emission such that texture nodes are registered
inline std::shared_ptr<Object> add_light(const std::shared_ptr<Object>& elem, GenContext& ctx)
{
	for (const auto& child : elem->namedChildren()) {
		if (child.second->type() == OT_TEXTURE)
			ctx.Textures.insert(child.second);
	}

	return elem;
}

inline constexpr std::array<uint32_t, 8> map_rectangle_index(const std::array<uint32_t, 4>& points)
{
	return { points[0], points[1], points[2], 0, points[2], points[3], points[0], 0 };
}

template <size_t N>
inline void insert_index(TriMesh& mesh, const std::array<uint32_t, N>& arr)
{
	mesh.indices.insert(mesh.indices.end(), arr.begin(), arr.end());
}

inline void add_rectangle(TriMesh& mesh, const std::array<Vector3f, 4>& points, const Vector3f& N)
{
	uint32_t off = mesh.vertices.size();
	mesh.vertices.insert(mesh.vertices.end(), points.begin(), points.end());
	mesh.normals.insert(mesh.normals.end(), { N, N, N, N });
	mesh.texcoords.insert(mesh.texcoords.end(), { Vector2f(0, 0), Vector2f(0, 1), Vector2f(1, 1), Vector2f(1, 0) });
	mesh.face_normals.insert(mesh.face_normals.end(), { N, N });
	insert_index(mesh, map_rectangle_index({ 0 + off, 1 + off, 2 + off, 3 + off }));
}

inline TriMesh setup_mesh_rectangle(const Object& elem, const LoadInfo& info)
{
	const Vector3f N = Vector3f(0, 0, 1);
	TriMesh mesh;
	add_rectangle(mesh, { Vector3f(-1, -1, 0), Vector3f(1, -1, 0), Vector3f(1, 1, 0), Vector3f(-1, 1, 0) }, N);
	return mesh;
}

inline TriMesh setup_mesh_cube(const Object& elem, const LoadInfo& info)
{
	const Vector3f NZ = Vector3f(0, 0, 1);
	const Vector3f NY = Vector3f(0, 1, 0);
	const Vector3f NX = Vector3f(1, 0, 0);

	// TODO: Fix order (is it?)
	TriMesh mesh;
	add_rectangle(mesh, { Vector3f(-1, -1, -1), Vector3f(1, -1, -1), Vector3f(1, 1, -1), Vector3f(-1, 1, -1) }, -NZ);
	add_rectangle(mesh, { Vector3f(-1, -1, 1), Vector3f(-1, 1, 1), Vector3f(1, 1, 1), Vector3f(1, -1, 1) }, NZ);

	add_rectangle(mesh, { Vector3f(1, -1, -1), Vector3f(1, 1, -1), Vector3f(1, 1, 1), Vector3f(1, -1, 1) }, NX);
	add_rectangle(mesh, { Vector3f(-1, -1, -1), Vector3f(-1, -1, 1), Vector3f(-1, 1, 1), Vector3f(-1, 1, -1) }, -NX);

	add_rectangle(mesh, { Vector3f(-1, -1, -1), Vector3f(1, -1, -1), Vector3f(1, -1, 1), Vector3f(-1, -1, 1) }, -NY);
	add_rectangle(mesh, { Vector3f(-1, 1, -1), Vector3f(-1, 1, 1), Vector3f(1, 1, 1), Vector3f(1, 1, -1) }, NY);
	return mesh;
}

inline TriMesh setup_mesh_obj(const Object& elem, const LoadInfo& info)
{
	const auto filename = info.handlePath(elem.property("filename").getString());
	auto trimesh		= obj::load(filename, 0);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << "" << std::endl;
		return TriMesh();
	}

	return trimesh;
}

inline TriMesh setup_mesh_ply(const Object& elem, const LoadInfo& info)
{
	const auto filename = info.handlePath(elem.property("filename").getString());
	auto trimesh		= ply::load(filename);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << "" << std::endl;
		return TriMesh();
	}
	return trimesh;
}

inline TriMesh setup_mesh_serialized(const Object& elem, const LoadInfo& info)
{
	size_t shape_index	= elem.property("shape_index").getInteger(0);
	const auto filename = info.handlePath(elem.property("filename").getString());
	auto trimesh		= mts::load(filename, shape_index);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << "" << std::endl;
		return TriMesh();
	}
	return trimesh;
}

static void setup_shapes(const Object& elem, const LoadInfo& info, GenContext& ctx, std::ostream& os)
{
	std::unordered_map<Material, uint32_t, MaterialHash> unique_mats;

	for (const auto& child : elem.anonymousChildren()) {
		if (child->type() != OT_SHAPE)
			continue;

		TriMesh child_mesh;
		if (child->pluginType() == "rectangle") {
			child_mesh = setup_mesh_rectangle(*child, info);
		} else if (child->pluginType() == "cube") {
			child_mesh = setup_mesh_cube(*child, info);
		} else if (child->pluginType() == "obj") {
			child_mesh = setup_mesh_obj(*child, info);
		} else if (child->pluginType() == "ply") {
			child_mesh = setup_mesh_ply(*child, info);
		} else if (child->pluginType() == "serialized") {
			child_mesh = setup_mesh_serialized(*child, info);
		} else {
			IG_LOG(L_WARNING) << "Can not load shape type '" << child->pluginType() << "'" << std::endl;
			continue;
		}

		if (child_mesh.vertices.empty())
			continue;

		auto flip = child->property("flip_normals").getBool();
		if (flip)
			child_mesh.flipNormals();

		auto transform = child->property("to_world").getTransform();
		for (size_t i = 0; i < child_mesh.vertices.size(); ++i)
			child_mesh.vertices[i] = applyTransformAffine(transform, child_mesh.vertices[i]);
		for (size_t i = 0; i < child_mesh.normals.size(); ++i)
			child_mesh.normals[i] = applyNormalTransform(transform, child_mesh.normals[i]);
		for (size_t i = 0; i < child_mesh.face_normals.size(); ++i)
			child_mesh.face_normals[i] = applyNormalTransform(transform, child_mesh.face_normals[i]);

		Shape shape;
		shape.VtxOffset = ctx.Mesh.vertices.size();
		shape.ItxOffset = ctx.Mesh.indices.size();
		shape.VtxCount	= child_mesh.vertices.size();
		shape.ItxCount	= child_mesh.indices.size();

		// Setup material & light
		for (const auto& inner_child : child->anonymousChildren()) {
			if (inner_child->type() == OT_BSDF)
				shape.Material.BSDF = add_bsdf(inner_child, ctx);
			else if (inner_child->type() == OT_EMITTER) {
				shape.Material.Light  = add_light(inner_child, ctx);
				shape.Material.MeshId = ctx.Shapes.size();
			}
		}

		for (const auto& inner_child : child->namedChildren()) {
			if (inner_child.second->type() == OT_BSDF)
				shape.Material.BSDF = add_bsdf(inner_child.second, ctx);
			else if (inner_child.second->type() == OT_EMITTER) {
				shape.Material.Light  = add_light(inner_child.second, ctx);
				shape.Material.MeshId = ctx.Shapes.size();
			}
		}

		if (!unique_mats.count(shape.Material)) {
			unique_mats.emplace(shape.Material, ctx.Materials.size());
			ctx.Materials.emplace_back(shape.Material);
		}

		child_mesh.replaceMaterial(unique_mats.at(shape.Material));
		ctx.Mesh.mergeFrom(child_mesh);
		ctx.Shapes.emplace_back(std::move(shape));
	}

	if (ctx.Shapes.empty()) {
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
	   << "        triangles    = @ |i| { let (i, j, k, _) = indices.load_int4(i); (i, j, k) },\n"
	   << "        attrs        = @ |_| (false, @ |j| vec2_to_4(texcoords.load_vec2(j), 0.0, 0.0)),\n"
	   << "        num_attrs    = 1,\n"
	   << "        num_tris     = " << size_t(ctx.Mesh.indices.size() / 4) << "\n"
	   << "    };\n"
	   << "    let bvh = device.load_bvh(\"data/bvh.bin\");\n";

	if (ctx.Mesh.face_area.size() < 4) // Make sure it is not too small
		ctx.Mesh.face_area.resize(16);
	IO::write_tri_mesh(ctx.Mesh, info.EnablePadding);

	// Generate BVHs
	if (IO::must_build_bvh(info.FilePath.string(), info.Target)) {
		IG_LOG(L_INFO) << "Generating BVH for '" << info.FilePath << "'" << std::endl;
		std::remove("data/bvh.bin");
		if (info.Target == Target::NVVM_STREAMING || info.Target == Target::NVVM_MEGAKERNEL || info.Target == Target::AMDGPU_STREAMING || info.Target == Target::AMDGPU_MEGAKERNEL) {
			std::vector<typename BvhNTriM<2, 1>::Node> nodes;
			std::vector<typename BvhNTriM<2, 1>::Tri> tris;
			build_bvh<2, 1>(ctx.Mesh, nodes, tris);
			IO::write_bvh(nodes, tris);
		} else if (info.Target == Target::GENERIC || info.Target == Target::ASIMD || info.Target == Target::SSE42) {
			std::vector<typename BvhNTriM<4, 4>::Node> nodes;
			std::vector<typename BvhNTriM<4, 4>::Tri> tris;
			build_bvh<4, 4>(ctx.Mesh, nodes, tris);
			IO::write_bvh(nodes, tris);
		} else {
			std::vector<typename BvhNTriM<8, 4>::Node> nodes;
			std::vector<typename BvhNTriM<8, 4>::Tri> tris;
			build_bvh<8, 4>(ctx.Mesh, nodes, tris);
			IO::write_bvh(nodes, tris);
		}
		std::ofstream bvh_stamp("data/bvh.stamp");
		bvh_stamp << int(info.Target) << " " << info.FilePath;
	} else {
		IG_LOG(L_INFO) << "Reusing existing BVH for '" << info.FilePath << "'" << std::endl;
	}

	// Calculate scene bounding box
	ctx.SceneBBox = BoundingBox::Empty();
	for (size_t i = 0; i < ctx.Mesh.vertices.size(); ++i)
		ctx.SceneBBox.extend(ctx.Mesh.vertices[i]);
	ctx.SceneDiameter = (ctx.SceneBBox.max - ctx.SceneBBox.min).norm();
}

static void load_texture(const std::string& filename, const LoadInfo& info, const GenContext& ctx, std::ostream& os)
{
	const auto c_name = info.handlePath(filename);
	const auto id	  = info.makeId(filename);
	os << "    let image_" << id << " = device.load_img(\"" << c_name.string() << "\");\n";
	os << "    let tex_" << id << " = make_texture(math, make_repeat_border(), make_bilinear_filter(), image_" << id << ");\n";
}

static void setup_textures(const Object& elem, const LoadInfo& info, const GenContext& ctx, std::ostream& os)
{
	if (ctx.Textures.empty())
		return;

	std::unordered_set<std::string> mapped;

	IG_LOG(L_INFO) << "Generating images for '" << info.FilePath << "'" << std::endl;
	os << "\n    // Images\n";
	for (const auto& tex : ctx.Textures) {
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
		load_texture(filename, info, ctx, os);
	}
}

static std::string extractTexture(const std::shared_ptr<Object>& tex, const LoadInfo& info, const GenContext& ctx);
static std::string extractMaterialPropertyColor(const std::shared_ptr<Object>& obj, const std::string& name, const LoadInfo& info, const GenContext& ctx, float def = 0.0f)
{
	std::stringstream sstream;

	auto prop = obj->property(name);
	if (prop.isValid()) {
		switch (prop.type()) {
		case PT_INTEGER:
			sstream << "make_gray_color(" << escape_f32(prop.getInteger()) << ")";
			break;
		case PT_NUMBER:
			sstream << "make_gray_color(" << escape_f32(prop.getNumber()) << ")";
			break;
		case PT_RGB: {
			auto v_rgb = prop.getRGB();
			sstream << "make_color(" << escape_f32(v_rgb.r) << ", " << escape_f32(v_rgb.r) << ", " << escape_f32(v_rgb.r) << ")";
		} break;
		case PT_SPECTRUM: {
			// TODO
			IG_LOG(L_WARNING) << "No support for spectrums" << std::endl;
			sstream << "black";
		} break;
		case PT_BLACKBODY: {
			// TODO
			IG_LOG(L_WARNING) << "No support for blackbodies" << std::endl;
			sstream << "black";
		} break;
		default:
			IG_LOG(L_WARNING) << "Unknown property type for '" << name << "'" << std::endl;
			sstream << "black";
			break;
		}
	} else {
		auto tex = obj->namedChild(name);
		if (!tex) {
			sstream << "make_gray_color(" << escape_f32(def) << ")";
		} else {
			if (tex->type() == OT_TEXTURE) {
				sstream << extractTexture(tex, info, ctx);
			} else {
				IG_LOG(L_WARNING) << "Invalid child type" << std::endl;
				sstream << "black";
			}
		}
	}

	return sstream.str();
}

static std::string extractTexture(const std::shared_ptr<Object>& tex, const LoadInfo& info, const GenContext& ctx)
{
	std::stringstream sstream;
	if (tex->pluginType() == "bitmap") {
		std::string filename = tex->property("filename").getString();
		if (filename.empty()) {
			IG_LOG(L_WARNING) << "Invalid texture found" << std::endl;
			sstream << "black";
		} else {
			sstream << "tex_" << info.makeId(filename) << "(vec4_to_2(surf.attr(0)))";
		}
	} else if (tex->pluginType() == "checkerboard") {
		auto uscale = tex->property("uscale").getNumber(1);
		auto vscale = tex->property("vscale").getNumber(1);
		sstream << "eval_checkerboard_texture(math, make_repeat_border(), "
				<< extractMaterialPropertyColor(tex, "color0", info, ctx, 0.4f) << ", "
				<< extractMaterialPropertyColor(tex, "color1", info, ctx, 0.2f) << ", ";
		if (uscale != 1 || vscale != 1)
			sstream << "vec2_mul(vec4_to_2(surf.attr(0)), make_vec2(" << escape_f32(uscale) << ", " << escape_f32(vscale) << "))";
		else
			sstream << "vec4_to_2(surf.attr(0))";

		sstream << ")";
	} else {
		IG_LOG(L_WARNING) << "Invalid texture type '" << tex->pluginType() << "'" << std::endl;
	}
	return sstream.str();
}

static void setup_materials(const Object& elem, const LoadInfo& info, const GenContext& ctx, std::ostream& os)
{
	if (ctx.Materials.empty())
		return;

	IG_LOG(L_INFO) << "Generating lights for '" << info.FilePath << "'" << std::endl;
	os << "\n    // Emission\n";
	size_t light_counter = 0;
	for (const auto& mat : ctx.Materials) {
		if (!mat.Light)
			continue;

		const auto& shape = ctx.Shapes[mat.MeshId];
		os << "    let light_" << light_counter << " = make_trimesh_light(math, tri_mesh, "
		   << (shape.ItxOffset / 4) << ", " << (shape.ItxCount / 4) << ", "
		   << extractMaterialPropertyColor(mat.Light, "radiance", info, ctx) << ");\n";
		++light_counter;
	}

	IG_LOG(L_INFO) << "Generating materials for '" << info.FilePath << "'" << std::endl;
	os << "\n    // Materials\n";

	light_counter = 0;
	for (size_t i = 0; i < ctx.Materials.size(); ++i) {
		const auto& mat = ctx.Materials[i];
		os << "    let material_" << i << " : Shader = @ |ray, hit, surf| {\n";
		if (!mat.BSDF) {
			os << "        let bsdf = make_black_bsdf();\n";
		} else if (mat.BSDF->pluginType() == "diffuse" || mat.BSDF->pluginType() == "roughdiffuse" /*TODO*/) {
			os << "        let bsdf = make_diffuse_bsdf(math, surf, " << extractMaterialPropertyColor(mat.BSDF, "reflectance", info, ctx) << ");\n";
		} else if (mat.BSDF->pluginType() == "dielectric" || mat.BSDF->pluginType() == "roughdielectric" /*TODO*/ || mat.BSDF->pluginType() == "thindielectric" /*TODO*/) {
			os << "        let bsdf = make_glass_bsdf(math, surf, "
			   << escape_f32(mat.BSDF->property("ext_ior").getNumber(1.000277f)) << ", "
			   << escape_f32(mat.BSDF->property("int_ior").getNumber(1.5046f)) << ", "
			   << extractMaterialPropertyColor(mat.BSDF, "specular_reflectance", info, ctx, 1.0f) << ", "
			   << extractMaterialPropertyColor(mat.BSDF, "specular_transmittance", info, ctx, 1.0f) << ");\n";
		} else if (mat.BSDF->pluginType() == "conductor" || mat.BSDF->pluginType() == "roughconductor" /*TODO*/) {
			os << "        let bsdf = make_conductor_bsdf(math, surf, "
			   << extractMaterialPropertyColor(mat.BSDF, "eta", info, ctx, 0.63660f) << ", "
			   << extractMaterialPropertyColor(mat.BSDF, "k", info, ctx, 2.7834f) << ", " // TODO: Better defaults?
			   << extractMaterialPropertyColor(mat.BSDF, "specular_reflectance", info, ctx, 1.0f) << ");\n";
		} else if (mat.BSDF->pluginType() == "phong" || mat.BSDF->pluginType() == "plastic" /*TODO*/ || mat.BSDF->pluginType() == "roughplastic" /*TODO*/) {
			os << "        let bsdf = make_phong_bsdf(math, surf, "
			   << extractMaterialPropertyColor(mat.BSDF, "specular_reflectance", info, ctx, 1.0f) << ", "
			   << escape_f32(mat.BSDF->property("exponent").getNumber(30)) << ");\n";
		} else if (mat.BSDF->pluginType() == "null") {
			os << "        let bsdf = make_black_bsdf();/* Null */\n";
		} else {
			IG_LOG(L_WARNING) << "Unknown bsdf '" << mat.BSDF->pluginType() << "'" << std::endl;
			os << "        let bsdf = make_black_bsdf();\n";
		}

		if (mat.Light)
			os << "        make_emissive_material(surf, bsdf, light_" << light_counter << ")\n";
		else
			os << "        make_material(bsdf)\n";
		os << "    };\n";

		if (mat.Light)
			++light_counter;
	}
}

static size_t setup_lights(const Object& elem, const LoadInfo& info, const GenContext& ctx, std::ostream& os)
{
	IG_LOG(L_INFO) << "Generating lights for '" << info.FilePath << "'" << std::endl;
	size_t light_count = 0;
	// Make sure area lights are the first ones
	for (const auto& m : ctx.Materials) {
		if (m.Light)
			++light_count;
	}

	for (const auto& child : elem.anonymousChildren()) {
		if (child->type() != OT_EMITTER)
			continue;

		if (child->pluginType() == "point") {
			auto pos = child->property("position").getVector();
			os << "    let light_" << light_count << " = make_point_light(math, make_vec3("
			   << escape_f32(pos.x) << ", " << escape_f32(pos.y) << ", " << escape_f32(pos.z) << "), "
			   << extractMaterialPropertyColor(child, "intensity", info, ctx, 1.0f) << ");\n";
		} else if (child->pluginType() == "area") {
			IG_LOG(L_WARNING) << "Area emitter without a shape is not allowed" << std::endl;
			continue;
		} else if (child->pluginType() == "directional") { // TODO: By to_world?
			auto dir = child->property("direction").getVector();
			os << "    let light_" << light_count << " = make_directional_light(math, make_vec3("
			   << escape_f32(dir.x) << ", " << escape_f32(dir.y) << ", " << escape_f32(dir.z) << "), "
			   << escape_f32(ctx.SceneDiameter) << ", "
			   << extractMaterialPropertyColor(child, "irradiance", info, ctx, 1.0f) << ");\n";
		} else if (child->pluginType() == "sun" || child->pluginType() == "sunsky") { // TODO
			IG_LOG(L_WARNING) << "Sun emitter is approximated by directional light" << std::endl;
			auto dir   = child->property("sun_direction").getVector();
			auto power = child->property("scale").getNumber(1.0f);
			os << "    let light_" << light_count << " = make_directional_light(math, make_vec3("
			   << escape_f32(dir.x) << ", " << escape_f32(dir.y) << ", " << escape_f32(dir.z) << "), "
			   << escape_f32(ctx.SceneDiameter) << ", "
			   << "make_d65_illum(" << escape_f32(power) << "));\n";
		} else if (child->pluginType() == "constant") {
			os << "    let light_" << light_count << " = make_environment_light(math, "
			   << escape_f32(ctx.SceneDiameter) << ", "
			   << extractMaterialPropertyColor(child, "radiance", info, ctx, 1.0f) << ");\n";
		} else if (child->pluginType() == "envmap") {
			auto filename = child->property("filename").getString();
			load_texture(filename, info, ctx, os); // TODO: What if filename is used already in the previous material stages
			os << "    let light_" << light_count << " = make_environment_light_textured(math, "
			   << escape_f32(ctx.SceneDiameter) << ", "
			   << "tex_" << info.makeId(filename) << ");\n";
		} else {
			IG_LOG(L_WARNING) << "Unknown emitter type '" << child->pluginType() << "'" << std::endl;
			continue;
		}

		++light_count;
	}

	os << "\n    // Lights\n";
	if (light_count == 0) { // Camera light
		os << "    let lights = @ |_ : i32| make_camera_light(math, camera, make_spectrum_identity());\n";
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

static void convert_scene(const Scene& scene, const LoadInfo& info, std::ostream& os)
{
	GenContext ctx;

	setup_integrator(scene, info, os);
	setup_camera(scene, info, os);
	setup_shapes(scene, info, ctx, os);
	setup_textures(scene, info, ctx, os);
	setup_materials(scene, info, ctx, os);
	size_t light_count = setup_lights(scene, info, ctx, os);

	os << "\n    // Geometries\n"
	   << "    let geometries = @ |i : i32| match i {\n";
	for (uint32_t s = 0; s < ctx.Materials.size(); ++s) {
		os << "        ";
		if (s != ctx.Materials.size() - 1)
			os << s;
		else
			os << "_";
		os << " => make_tri_mesh_geometry(math, tri_mesh, material_" << s << "),\n";
	}
	os << "    };\n";

	os << "\n    // Scene\n"
	   << "    let scene = Scene {\n"
	   << "        num_geometries = " << ctx.Materials.size() << ",\n"
	   << "        num_lights     = " << light_count << ",\n"
	   << "        geometries     = @ |i : i32| geometries(i),\n"
	   << "        lights         = @ |i : i32| lights(i),\n"
	   << "        camera         = camera,\n"
	   << "        bvh            = bvh\n"
	   << "    };\n";
}

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

		LoadInfo info;
		info.FilePath	   = std::filesystem::canonical(filepath);
		info.Target		   = options.target;
		info.MaxPathLen	   = options.max_path_length;
		info.SPP		   = options.spp;
		info.Fusion		   = options.fusion;
		info.EnablePadding = require_padding(options.target);

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

		convert_scene(scene, info, os);

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