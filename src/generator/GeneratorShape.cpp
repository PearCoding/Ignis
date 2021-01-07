#include "GeneratorShape.h"

#include "Logger.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

#include "BVHIO.h"
#include "IO.h"
#include "TriBVHAdapter.h"

#include <sstream>

namespace IG {

using namespace Loader;

struct TransformCache {
	Matrix4f TransformMatrix;
	Matrix3f NormalMatrix;

	TransformCache(const Transformf& t)
	{
		TransformMatrix = t.matrix();
		NormalMatrix	= TransformMatrix.block<3, 3>(0, 0).transpose().inverse();
	}

	inline Vector3f applyTransform(const Vector3f& v) const
	{
		Vector4f w = TransformMatrix * Vector4f(v(0), v(1), v(2), 1.0f);
		w /= w(3);
		return Vector3f(w(0), w(1), w(2));
	}

	inline Vector3f applyNormal(const Vector3f& n) const
	{
		return (NormalMatrix * n).normalized();
	}
};

inline constexpr std::array<uint32_t, 8> map_rectangle_index(const std::array<uint32_t, 4>& points)
{
	return { points[0], points[1], points[2], 0, points[2], points[3], points[0], 0 };
}

template <size_t N>
inline void insert_index(TriMesh& mesh, const std::array<uint32_t, N>& arr)
{
	mesh.indices.insert(mesh.indices.end(), arr.begin(), arr.end());
}

inline float triangle_area(const std::array<StVector3f, 3>& points)
{
	return 0.5f * (points[1] - points[0]).cross(points[2] - points[0]).norm();
}

inline void add_rectangle(TriMesh& mesh, const std::array<StVector3f, 4>& points, const StVector3f& N)
{
	uint32_t off = mesh.vertices.size();
	mesh.vertices.insert(mesh.vertices.end(), points.begin(), points.end());
	mesh.normals.insert(mesh.normals.end(), { N, N, N, N });
	mesh.texcoords.insert(mesh.texcoords.end(), { StVector2f(0, 0), StVector2f(0, 1), StVector2f(1, 1), StVector2f(1, 0) });
	mesh.face_normals.insert(mesh.face_normals.end(), { N, N });
	mesh.face_area.insert(mesh.face_area.end(), { triangle_area({ points[0], points[1], points[2] }), triangle_area({ points[2], points[3], points[0] }) });
	insert_index(mesh, map_rectangle_index({ 0 + off, 1 + off, 2 + off, 3 + off }));
}

inline TriMesh setup_mesh_rectangle(const Object& elem)
{
	const Vector3f N = Vector3f(0, 0, 1);
	TriMesh mesh;
	add_rectangle(mesh, { Vector3f(-1, -1, 0), Vector3f(1, -1, 0), Vector3f(1, 1, 0), Vector3f(-1, 1, 0) }, N);
	return mesh;
}

inline TriMesh setup_mesh_cube(const Object& elem)
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

inline TriMesh setup_mesh_obj(const Object& elem, const GeneratorContext& ctx)
{
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_INFO) << "Trying to load obj file " << filename << std::endl;
	auto trimesh = obj::load(filename, 0);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}

	return trimesh;
}

inline TriMesh setup_mesh_ply(const Object& elem, const GeneratorContext& ctx)
{
	IG_LOG(L_INFO) << "Trying to load ply file " << elem.property("filename").getString() << std::endl;
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_INFO) << "Trying to load ply file " << filename << std::endl;
	auto trimesh = ply::load(filename);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}
	return trimesh;
}

inline TriMesh setup_mesh_mitsuba(const Object& elem, const GeneratorContext& ctx)
{
	size_t shape_index	= elem.property("shape_index").getInteger(0);
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_INFO) << "Trying to load serialized mitsuba file " << filename << std::endl;
	auto trimesh = mts::load(filename, shape_index);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}
	return trimesh;
}

void GeneratorShape::setup(GeneratorContext& ctx)
{
	for (const auto& pair : ctx.Scene.shapes()) {
		const auto child = pair.second;

		TriMesh child_mesh;
		if (child->pluginType() == "rectangle") {
			child_mesh = setup_mesh_rectangle(*child);
		} else if (child->pluginType() == "cube") {
			child_mesh = setup_mesh_cube(*child);
		} else if (child->pluginType() == "obj") {
			child_mesh = setup_mesh_obj(*child, ctx);
		} else if (child->pluginType() == "ply") {
			child_mesh = setup_mesh_ply(*child, ctx);
		} else if (child->pluginType() == "mitsuba") {
			child_mesh = setup_mesh_mitsuba(*child, ctx);
		} else {
			IG_LOG(L_WARNING) << "Can not load shape type '" << child->pluginType() << "'" << std::endl;
			continue;
		}

		if (child_mesh.vertices.empty()) {
			IG_LOG(L_WARNING) << "While loading shape type '" << child->pluginType() << "' no vertices were generated" << std::endl;
			continue;
		}

		if (child->property("flip_normals").getBool())
			child_mesh.flipNormals();

		if (child->property("face_normals").getBool())
			child_mesh.setupFaceNormalsAsVertexNormals();

		TransformCache transform = TransformCache(child->property("to_world").getTransform());
		for (size_t i = 0; i < child_mesh.vertices.size(); ++i)
			child_mesh.vertices[i] = transform.applyTransform(child_mesh.vertices[i]);
		for (size_t i = 0; i < child_mesh.normals.size(); ++i)
			child_mesh.normals[i] = transform.applyNormal(child_mesh.normals[i]);
		for (size_t i = 0; i < child_mesh.face_normals.size(); ++i)
			child_mesh.face_normals[i] = transform.applyNormal(child_mesh.face_normals[i]);

		// Build bounding box
		BoundingBox bbox = BoundingBox::Empty();
		for (const auto& v : child_mesh.vertices)
			bbox.extend(v);
		
		bbox.inflate(0.0001f); // Make sure it has a volume

		const std::string suffix = GeneratorContext::makeId(pair.first);

		Shape shape;
		shape.VtxCount	  = child_mesh.vertices.size();
		shape.ItxCount	  = child_mesh.indices.size();
		shape.BoundingBox = bbox;
		shape.TriMesh	  = "trimesh_" + suffix;
		shape.BVH		  = "bvh_" + suffix;
		ctx.Environment.Shapes.insert({ pair.first, shape });

		// Export data:
		IG_LOG(L_INFO) << "Generating triangle mesh for shape " << pair.first << std::endl;
		if (child_mesh.face_area.size() < 4) // Make sure it is not too small
			child_mesh.face_area.resize(16);
		IO::write_tri_mesh(suffix, child_mesh, ctx.EnablePadding);

		// Generate BVH
		if (ctx.ForceBVHBuild || IO::must_build_bvh(suffix, ctx.FilePath.string(), ctx.Target)) {
			IG_LOG(L_INFO) << "Generating BVH for shape " << pair.first << std::endl;
			IO::remove_bvh(suffix);
			if (ctx.Target == Target::NVVM_STREAMING || ctx.Target == Target::NVVM_MEGAKERNEL || ctx.Target == Target::AMDGPU_STREAMING || ctx.Target == Target::AMDGPU_MEGAKERNEL) {
				std::vector<typename BvhNTriM<2, 1>::Node> nodes;
				std::vector<typename BvhNTriM<2, 1>::Tri> tris;
				build_bvh<2, 1>(child_mesh, nodes, tris);
				IO::write_bvh(suffix, nodes, tris);
			} else if (ctx.Target == Target::GENERIC || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
				std::vector<typename BvhNTriM<4, 4>::Node> nodes;
				std::vector<typename BvhNTriM<4, 4>::Tri> tris;
				build_bvh<4, 4>(child_mesh, nodes, tris);
				IO::write_bvh(suffix, nodes, tris);
			} else {
				std::vector<typename BvhNTriM<8, 4>::Node> nodes;
				std::vector<typename BvhNTriM<8, 4>::Tri> tris;
				build_bvh<8, 4>(child_mesh, nodes, tris);
				IO::write_bvh(suffix, nodes, tris);
			}
			IO::write_bvh_stamp(suffix, ctx.FilePath.string(), ctx.Target);
		} else {
			IG_LOG(L_INFO) << "Reusing existing BVH for shape " << pair.first << std::endl;
		}
	}
}

std::string GeneratorShape::dump(const GeneratorContext& ctx)
{
	std::stringstream sstream;

	for (const auto& pair : ctx.Scene.shapes()) {
		const std::string suffix = GeneratorContext::makeId(pair.first);
		const size_t numTris	 = ctx.Environment.Shapes.at(pair.first).ItxCount;

		sstream << "    // ---- Shape " << pair.first << "\n"
				<< "    let vertices_" << suffix << " = device.load_buffer(\"data/meshes/vertices_" << suffix << ".bin\");\n"
				<< "    let normals_" << suffix << " = device.load_buffer(\"data/meshes/normals_" << suffix << ".bin\");\n"
				<< "    let face_normals_" << suffix << " = device.load_buffer(\"data/meshes/face_normals_" << suffix << ".bin\");\n"
				<< "    let face_area_" << suffix << " = device.load_buffer(\"data/meshes/face_area_" << suffix << ".bin\");\n"
				<< "    let texcoords_" << suffix << " = device.load_buffer(\"data/meshes/texcoords_" << suffix << ".bin\");\n"
				<< "    let indices_" << suffix << " = device.load_buffer(\"data/meshes/indices_" << suffix << ".bin\");\n"
				<< "    let trimesh_" << suffix << " = TriMesh {\n"
				<< "       vertices     = @ |i| vertices_" << suffix << ".load_vec3(i),\n"
				<< "       normals      = @ |i| normals_" << suffix << ".load_vec3(i),\n"
				<< "       face_normals = @ |i| face_normals_" << suffix << ".load_vec3(i),\n"
				<< "       face_area    = @ |i| face_area_" << suffix << ".load_f32(i),\n"
				<< "       triangles    = @ |i| { let (i0, i1, i2, _) = indices_" << suffix << ".load_int4(i); (i0, i1, i2) },\n"
				<< "       attrs        = @ |_| (false, @ |j| vec2_to_4(texcoords_" << suffix << ".load_vec2(j), 0.0, 0.0)),\n"
				<< "       num_attrs    = 1,\n"
				<< "       num_tris     = " << numTris << "\n"
				<< "    };\n"
				<< "    let bvh_" << suffix << " = device.load_tri_bvh(\"" << IO::bvh_filepath(suffix) << "\");\n";
	}

	return sstream.str();
}
} // namespace IG