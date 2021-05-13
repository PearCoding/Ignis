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

inline TriMesh setup_mesh_rectangle(const Object& elem)
{
	const float width	  = elem.property("width").getNumber(2.0f);
	const float height	  = elem.property("height").getNumber(2.0f);
	const Vector3f origin = elem.property("origin").getVector3(Vector3f(-width / 2, -height / 2, 0));
	return TriMesh::MakePlane(origin, Vector3f::UnitX() * width, Vector3f::UnitY() * height);
}

inline TriMesh setup_mesh_cube(const Object& elem)
{
	const float width	  = elem.property("width").getNumber(2.0f);
	const float height	  = elem.property("height").getNumber(2.0f);
	const float depth	  = elem.property("depth").getNumber(2.0f);
	const Vector3f origin = elem.property("origin").getVector3(Vector3f(-width / 2, -height / 2, -depth / 2));
	return TriMesh::MakeBox(origin, Vector3f::UnitX() * width, Vector3f::UnitY() * height, Vector3f::UnitZ() * depth);
}

inline TriMesh setup_mesh_sphere(const Object& elem)
{
	const Vector3f center = elem.property("center").getVector3();
	const float radius	  = elem.property("radius").getNumber(1.0f);
	const uint32 stacks	  = elem.property("stacks").getInteger(32);
	const uint32 slices	  = elem.property("slices").getInteger(16);
	return TriMesh::MakeSphere(center, radius, stacks, slices);
}

inline TriMesh setup_mesh_cylinder(const Object& elem)
{
	const Vector3f baseCenter = elem.property("p0").getVector3();
	const Vector3f tipCenter  = elem.property("p1").getVector3(Vector3f(0, 0, 1));
	const float radius		  = elem.property("radius").getNumber(1.0f);
	const uint32 sections	  = elem.property("sections").getInteger(32);
	const bool filled		  = elem.property("filled").getBool(true);
	return TriMesh::MakeCylinder(baseCenter, radius, tipCenter, radius, sections, filled);
}

inline TriMesh setup_mesh_cone(const Object& elem)
{
	const Vector3f baseCenter = elem.property("p0").getVector3();
	const Vector3f tipCenter  = elem.property("p1").getVector3(Vector3f(0, 0, 1));
	const float radius		  = elem.property("radius").getNumber(1.0f);
	const uint32 sections	  = elem.property("sections").getInteger(32);
	const bool filled		  = elem.property("filled").getBool(true);
	return TriMesh::MakeCone(baseCenter, radius, tipCenter, sections, filled);
}

inline TriMesh setup_mesh_disk(const Object& elem)
{
	const Vector3f origin = elem.property("origin").getVector3();
	const Vector3f normal = elem.property("normal").getVector3(Vector3f(0, 0, 1));
	const float radius	  = elem.property("radius").getNumber(1.0f);
	const uint32 sections = elem.property("sections").getInteger(32);
	return TriMesh::MakeDisk(origin, normal, radius, sections);
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
		} else if (child->pluginType() == "sphere") {
			child_mesh = setup_mesh_sphere(*child);
		} else if (child->pluginType() == "cylinder") {
			child_mesh = setup_mesh_cylinder(*child);
		} else if (child->pluginType() == "cone") {
			child_mesh = setup_mesh_cone(*child);
		} else if (child->pluginType() == "disk") {
			child_mesh = setup_mesh_disk(*child);
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
		meshSerializer.write(child_mesh.indices, true);	  // Already aligned
		meshSerializer.write(child_mesh.texcoords, true); // Aligned to 4*2 bytes
		meshSerializer.write(child_mesh.face_inv_area, true);

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