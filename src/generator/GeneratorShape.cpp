#include "GeneratorShape.h"

#include "Logger.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

namespace IG {

using namespace TPM_NAMESPACE;

struct TransformCache {
	Matrix4f TransformMatrix;
	Matrix3f NormalMatrix;

	TransformCache(const Transform& t)
	{
		TransformMatrix = Eigen::Map<const Eigen::Matrix<float, 4, 4, Eigen::RowMajor>>(t.matrix.data());
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
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_INFO) << "Trying to load ply file " << filename << std::endl;
	auto trimesh = ply::load(filename);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}
	return trimesh;
}

inline TriMesh setup_mesh_serialized(const Object& elem, const GeneratorContext& ctx)
{
	size_t shape_index	= elem.property("shape_index").getInteger(0);
	const auto filename = ctx.handlePath(elem.property("filename").getString());
	IG_LOG(L_INFO) << "Trying to load serialized file " << filename << std::endl;
	auto trimesh = mts::load(filename, shape_index);
	if (trimesh.vertices.empty()) {
		IG_LOG(L_WARNING) << "Can not load shape given by file " << filename << std::endl;
		return TriMesh();
	}
	return trimesh;
}

void GeneratorShape::setup(const TPMObject& root, GeneratorContext& ctx)
{
	std::unordered_map<Material, uint32_t, MaterialHash> unique_mats;

	for (const auto& child : root.anonymousChildren()) {
		if (child->type() != OT_SHAPE)
			continue;

		TriMesh child_mesh;
		if (child->pluginType() == "rectangle") {
			child_mesh = setup_mesh_rectangle(*child);
		} else if (child->pluginType() == "cube") {
			child_mesh = setup_mesh_cube(*child);
		} else if (child->pluginType() == "obj") {
			child_mesh = setup_mesh_obj(*child, ctx);
		} else if (child->pluginType() == "ply") {
			child_mesh = setup_mesh_ply(*child, ctx);
		} else if (child->pluginType() == "serialized") {
			child_mesh = setup_mesh_serialized(*child, ctx);
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

		Shape shape;
		shape.VtxOffset = ctx.Environment.Mesh.vertices.size();
		shape.ItxOffset = ctx.Environment.Mesh.indices.size();
		shape.VtxCount	= child_mesh.vertices.size();
		shape.ItxCount	= child_mesh.indices.size();

		// Setup material & light
		shape.Material.MeshLightPairId = ctx.Environment.Shapes.size();
		for (const auto& inner_child : child->anonymousChildren()) {
			if (inner_child->type() == OT_BSDF) {
				shape.Material.BSDF = inner_child;
				ctx.registerTexturesFromBSDF(shape.Material.BSDF);
			} else if (inner_child->type() == OT_EMITTER) {
				shape.Material.Light = inner_child;
				ctx.registerTexturesFromLight(shape.Material.Light);
			}
		}

		for (const auto& inner_child : child->namedChildren()) {
			if (inner_child.second->type() == OT_BSDF) {
				shape.Material.BSDF = inner_child.second;
				ctx.registerTexturesFromBSDF(shape.Material.BSDF);
			} else if (inner_child.second->type() == OT_EMITTER) {
				shape.Material.Light = inner_child.second;
				ctx.registerTexturesFromLight(shape.Material.Light);
			}
		}

		if (!unique_mats.count(shape.Material)) {
			unique_mats.emplace(shape.Material, ctx.Environment.Materials.size());
			ctx.Environment.Materials.emplace_back(shape.Material);
		}

		child_mesh.replaceMaterial(unique_mats.at(shape.Material));
		ctx.Environment.Mesh.mergeFrom(child_mesh);
		ctx.Environment.Shapes.emplace_back(std::move(shape));
	}
}

} // namespace IG