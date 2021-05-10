#include "LoaderShape.h"
#include "Loader.h"

#include "Logger.h"
#include "bvh/TriBVHAdapter.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

#include "serialization/VectorSerializer.h"

#include <sstream>

namespace IG {

using namespace Parser;

struct TransformCache {
	Matrix4f TransformMatrix;
	Matrix3f NormalMatrix;

	TransformCache(const Transformf& t)
	{
		TransformMatrix = t.matrix();
		NormalMatrix	= TransformMatrix.transpose().inverse().block<3, 3>(0, 0);
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

inline TriMesh setup_mesh_obj(const Object& elem, const LoaderContext& ctx)
{
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_DEBUG) << "Trying to load obj file " << filename << std::endl;
	auto trimesh = obj::load(filename, 0);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}

	return trimesh;
}

inline TriMesh setup_mesh_ply(const Object& elem, const LoaderContext& ctx)
{
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_DEBUG) << "Trying to load ply file " << filename << std::endl;
	auto trimesh = ply::load(filename);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}
	return trimesh;
}

inline TriMesh setup_mesh_mitsuba(const Object& elem, const LoaderContext& ctx)
{
	size_t shape_index	= elem.property("shape_index").getInteger(0);
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_DEBUG) << "Trying to load serialized mitsuba file " << filename << std::endl;
	auto trimesh = mts::load(filename, shape_index);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}
	return trimesh;
}

template <size_t N, size_t T>
static void setup_prim_bvh(VectorSerializer& serializer, const TriMesh& mesh)
{
	std::vector<typename BvhNTriM<N, T>::Node> nodes;
	std::vector<typename BvhNTriM<N, T>::Tri> tris;
	build_bvh<N, T>(mesh, nodes, tris);
	serializer.write((uint32)nodes.size());
	serializer.write((uint32)0); // Padding
	serializer.write((uint32)0); // Padding
	serializer.write((uint32)0); // Padding
	serializer.write(nodes, true);
	serializer.write(tris, true);
}

bool LoaderShape::load(LoaderContext& ctx, LoaderResult& result)
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

		TransformCache transform = TransformCache(child->property("transform").getTransform());
		if (!transform.TransformMatrix.isIdentity()) {
			for (size_t i = 0; i < child_mesh.vertices.size(); ++i)
				child_mesh.vertices[i] = transform.applyTransform(child_mesh.vertices[i]);
			for (size_t i = 0; i < child_mesh.normals.size(); ++i)
				child_mesh.normals[i] = transform.applyNormal(child_mesh.normals[i]);
			for (size_t i = 0; i < child_mesh.face_normals.size(); ++i)
				child_mesh.face_normals[i] = transform.applyNormal(child_mesh.face_normals[i]);
		}

		// Build bounding box
		BoundingBox bbox = BoundingBox::Empty();
		for (const auto& v : child_mesh.vertices)
			bbox.extend(v);
		bbox.inflate(1e-5f); // Make sure it has a volume

		// Register shape into environment
		const uint32 shapeID = ctx.Environment.Shapes.size();
		Shape shape;
		shape.VtxCount	  = child_mesh.vertices.size();
		shape.ItxCount	  = child_mesh.indices.size();
		shape.BoundingBox = bbox;

		ctx.Environment.Shapes.push_back(shape);
		ctx.Environment.ShapeIDs[pair.first] = shapeID;

		IG_ASSERT(child_mesh.face_normals.size() == child_mesh.faceCount(), "Expected valid face normals!");
		IG_ASSERT((child_mesh.indices.size() % 4) == 0, "Expected index buffer count to be a multiple of 4!");

		// Export data:
		IG_LOG(L_DEBUG) << "Generating triangle mesh for shape " << pair.first << std::endl;
		
		auto& meshData = result.Database.ShapeTable.addLookup(0, DefaultAlignment); // TODO: No use of the typeid currently
		VectorSerializer meshSerializer(meshData, false);
		meshSerializer.write((uint32)child_mesh.faceCount());
		meshSerializer.write((uint32)child_mesh.vertices.size());
		meshSerializer.write((uint32)child_mesh.normals.size());
		meshSerializer.write((uint32)0);
		meshSerializer.write_aligned(child_mesh.vertices, DefaultAlignment, true);
		meshSerializer.write_aligned(child_mesh.normals, DefaultAlignment, true);
		meshSerializer.write_aligned(child_mesh.face_normals, DefaultAlignment, true);
		meshSerializer.write(child_mesh.indices, true); // Already aligned
		meshSerializer.write_aligned(child_mesh.texcoords, DefaultAlignment, true);
		meshSerializer.write(child_mesh.face_area, true);

		// Generate BVH
		IG_LOG(L_DEBUG) << "Generating BVH for shape " << pair.first << std::endl;
		auto& bvhData = result.Database.BVHTable.addLookup(0, DefaultAlignment);
		VectorSerializer bvhSerializer(bvhData, false);
		if (ctx.Target == Target::NVVM_STREAMING || ctx.Target == Target::NVVM_MEGAKERNEL || ctx.Target == Target::AMDGPU_STREAMING || ctx.Target == Target::AMDGPU_MEGAKERNEL) {
			setup_prim_bvh<2, 1>(bvhSerializer, child_mesh);
		} else if (ctx.Target == Target::GENERIC || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
			setup_prim_bvh<4, 4>(bvhSerializer, child_mesh);
		} else {
			setup_prim_bvh<8, 4>(bvhSerializer, child_mesh);
		}
	}

	return true;
}
} // namespace IG