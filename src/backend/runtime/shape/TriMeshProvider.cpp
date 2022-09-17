#include "TriMeshProvider.h"

#include "bvh/TriBVHAdapter.h"
#include "loader/LoaderResult.h"
#include "loader/LoaderShape.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"
#include "serialization/VectorSerializer.h"

#include "Logger.h"

#include <tbb/scalable_allocator.h>

namespace IG {

using namespace Parser;

inline TriMesh setup_mesh_triangle(const Object& elem)
{
    const Vector3f p0 = elem.property("p0").getVector3(Vector3f(0, 0, 0));
    const Vector3f p1 = elem.property("p1").getVector3(Vector3f(1, 0, 0));
    const Vector3f p2 = elem.property("p2").getVector3(Vector3f(0, 1, 0));
    return TriMesh::MakeTriangle(p0, p1, p2);
}

inline TriMesh setup_mesh_rectangle(const Object& elem)
{
    if (elem.properties().count("p0") == 0) {
        const float width     = elem.property("width").getNumber(2.0f);
        const float height    = elem.property("height").getNumber(2.0f);
        const Vector3f origin = elem.property("origin").getVector3(Vector3f(-width / 2, -height / 2, 0));
        return TriMesh::MakePlane(origin, Vector3f::UnitX() * width, Vector3f::UnitY() * height);
    } else {
        const Vector3f p0 = elem.property("p0").getVector3(Vector3f(-1, -1, 0));
        const Vector3f p1 = elem.property("p1").getVector3(Vector3f(1, -1, 0));
        const Vector3f p2 = elem.property("p2").getVector3(Vector3f(1, 1, 0));
        const Vector3f p3 = elem.property("p3").getVector3(Vector3f(-1, 1, 0));
        return TriMesh::MakeRectangle(p0, p1, p2, p3);
    }
}

inline TriMesh setup_mesh_cube(const Object& elem)
{
    const float width     = elem.property("width").getNumber(2.0f);
    const float height    = elem.property("height").getNumber(2.0f);
    const float depth     = elem.property("depth").getNumber(2.0f);
    const Vector3f origin = elem.property("origin").getVector3(Vector3f(-width / 2, -height / 2, -depth / 2));
    return TriMesh::MakeBox(origin, Vector3f::UnitX() * width, Vector3f::UnitY() * height, Vector3f::UnitZ() * depth);
}

inline TriMesh setup_mesh_ico_sphere(const Object& elem)
{
    const Vector3f center     = elem.property("center").getVector3();
    const float radius        = elem.property("radius").getNumber(1.0f);
    const uint32 subdivisions = elem.property("subdivisions").getInteger(4);
    return TriMesh::MakeIcoSphere(center, radius, subdivisions);
}

inline TriMesh setup_mesh_uv_sphere(const Object& elem)
{
    const Vector3f center = elem.property("center").getVector3();
    const float radius    = elem.property("radius").getNumber(1.0f);
    const uint32 stacks   = elem.property("stacks").getInteger(32);
    const uint32 slices   = elem.property("slices").getInteger(16);
    return TriMesh::MakeUVSphere(center, radius, stacks, slices);
}

inline TriMesh setup_mesh_cylinder(const Object& elem)
{
    const Vector3f baseCenter = elem.property("p0").getVector3();
    const Vector3f tipCenter  = elem.property("p1").getVector3(Vector3f(0, 0, 1));
    const uint32 sections     = elem.property("sections").getInteger(32);
    const bool filled         = elem.property("filled").getBool(true);

    float baseRadius = 0;
    float tipRadius  = 0;
    if (elem.properties().count("radius") != 0) {
        baseRadius = elem.property("radius").getNumber(1.0f);
        tipRadius  = baseRadius;
    } else {
        baseRadius = elem.property("bottom_radius").getNumber(1.0f);
        tipRadius  = elem.property("top_radius").getNumber(baseRadius);
    }
    return TriMesh::MakeCylinder(baseCenter, baseRadius, tipCenter, tipRadius, sections, filled);
}

inline TriMesh setup_mesh_cone(const Object& elem)
{
    const Vector3f baseCenter = elem.property("p0").getVector3();
    const Vector3f tipCenter  = elem.property("p1").getVector3(Vector3f(0, 0, 1));
    const float radius        = elem.property("radius").getNumber(1.0f);
    const uint32 sections     = elem.property("sections").getInteger(32);
    const bool filled         = elem.property("filled").getBool(true);
    return TriMesh::MakeCone(baseCenter, radius, tipCenter, sections, filled);
}

inline TriMesh setup_mesh_disk(const Object& elem)
{
    const Vector3f origin = elem.property("origin").getVector3();
    const Vector3f normal = elem.property("normal").getVector3(Vector3f(0, 0, 1));
    const float radius    = elem.property("radius").getNumber(1.0f);
    const uint32 sections = elem.property("sections").getInteger(32);
    return TriMesh::MakeDisk(origin, normal, radius, sections);
}

inline TriMesh setup_mesh_obj(const std::string& name, const Object& elem, const LoaderContext& ctx)
{
    int shape_index     = elem.property("shape_index").getInteger(-1);
    const auto filename = ctx.handlePath(elem.property("filename").getString(), elem);
    // IG_LOG(L_DEBUG) << "Shape '" << name << "': Trying to load obj file " << filename << std::endl;
    auto trimesh = obj::load(filename, shape_index < 0 ? std::nullopt : std::make_optional(shape_index));
    if (trimesh.vertices.empty()) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Can not load shape given by file " << filename << std::endl;
        return TriMesh();
    }

    return trimesh;
}

inline TriMesh setup_mesh_ply(const std::string& name, const Object& elem, const LoaderContext& ctx)
{
    const auto filename = ctx.handlePath(elem.property("filename").getString(), elem);
    // IG_LOG(L_DEBUG) << "Shape '" << name << "': Trying to load ply file " << filename << std::endl;
    auto trimesh = ply::load(filename);
    if (trimesh.vertices.empty()) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Can not load shape given by file " << filename << std::endl;
        return TriMesh();
    }
    return trimesh;
}

inline TriMesh setup_mesh_mitsuba(const std::string& name, const Object& elem, const LoaderContext& ctx)
{
    size_t shape_index  = elem.property("shape_index").getInteger(0);
    const auto filename = ctx.handlePath(elem.property("filename").getString(), elem);
    // IG_LOG(L_DEBUG) << "Shape '" << name << "': Trying to load serialized mitsuba file " << filename << std::endl;
    auto trimesh = mts::load(filename, shape_index);
    if (trimesh.vertices.empty()) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Can not load shape given by file " << filename << std::endl;
        return TriMesh();
    }
    return trimesh;
}

inline TriMesh setup_mesh_external(const std::string& name, const Object& elem, const LoaderContext& ctx)
{
    const auto filename = ctx.handlePath(elem.property("filename").getString(), elem);
    if (filename.empty()) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': No filename given" << std::endl;
        return {};
    }

    if (to_lowercase(filename.extension().u8string()) == ".obj")
        return setup_mesh_obj(name, elem, ctx);
    else if (to_lowercase(filename.extension().u8string()) == ".ply")
        return setup_mesh_ply(name, elem, ctx);
    else if (to_lowercase(filename.extension().u8string()) == ".mts" || to_lowercase(filename.extension().u8string()) == ".serialized")
        return setup_mesh_mitsuba(name, elem, ctx);
    else
        IG_LOG(L_ERROR) << "Shape '" << name << "': Can not determine type of external mesh for given " << filename << std::endl;
    return {};
}

template <size_t N, size_t T>
struct BvhTemporary {
    std::vector<typename BvhNTriM<N, T>::Node, tbb::scalable_allocator<typename BvhNTriM<N, T>::Node>> nodes;
    std::vector<typename BvhNTriM<N, T>::Tri, tbb::scalable_allocator<typename BvhNTriM<N, T>::Tri>> tris;
};

template <size_t N, size_t T>
static void setup_bvh(const TriMesh& mesh, SceneDatabase& dtb, std::mutex& mutex)
{
    BvhTemporary<N, T> bvh;
    if (mesh.faceCount() > 0)
        build_bvh<N, T>(mesh, bvh.nodes, bvh.tris);

    mutex.lock();
    auto& bvhTable = dtb.Tables["trimesh_primbvh"];
    auto& bvhData  = bvhTable.addLookup(0, 0, DefaultAlignment);
    VectorSerializer serializer(bvhData, false);
    serializer.write((uint32)bvh.nodes.size());
    serializer.write((uint32)bvh.tris.size()); // Not really needed, but just dump it out
    serializer.write((uint32)0);               // Padding
    serializer.write((uint32)0);               // Padding
    serializer.write(bvh.nodes, true);
    serializer.write(bvh.tris, true);
    mutex.unlock();
}

void TriMeshProvider::handle(LoaderContext& ctx, LoaderResult& result, const std::string& name, const Parser::Object& elem)
{
    TriMesh mesh;
    if (elem.pluginType() == "triangle") {
        mesh = setup_mesh_triangle(elem);
    } else if (elem.pluginType() == "rectangle") {
        mesh = setup_mesh_rectangle(elem);
    } else if (elem.pluginType() == "cube" || elem.pluginType() == "box") {
        mesh = setup_mesh_cube(elem);
    } else if (elem.pluginType() == "sphere" || elem.pluginType() == "icosphere") {
        mesh = setup_mesh_ico_sphere(elem);
    } else if (elem.pluginType() == "uvsphere") {
        mesh = setup_mesh_uv_sphere(elem);
    } else if (elem.pluginType() == "cylinder") {
        mesh = setup_mesh_cylinder(elem);
    } else if (elem.pluginType() == "cone") {
        mesh = setup_mesh_cone(elem);
    } else if (elem.pluginType() == "disk") {
        mesh = setup_mesh_disk(elem);
    } else if (elem.pluginType() == "obj") {
        mesh = setup_mesh_obj(name, elem, ctx);
    } else if (elem.pluginType() == "ply") {
        mesh = setup_mesh_ply(name, elem, ctx);
    } else if (elem.pluginType() == "mitsuba") {
        mesh = setup_mesh_mitsuba(name, elem, ctx);
    } else if (elem.pluginType() == "external") {
        mesh = setup_mesh_external(name, elem, ctx);
    } else {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Can not load shape type '" << elem.pluginType() << "'" << std::endl;
        return;
    }

    if (mesh.vertices.empty()) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': While loading shape type '" << elem.pluginType() << "' no vertices were generated" << std::endl;
        return;
    }

    if (mesh.faceCount() == 0) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': While loading shape type '" << elem.pluginType() << "' no indices were generated" << std::endl;
        return;
    }

    // Handle some user options
    if (elem.property("flip_normals").getBool())
        mesh.flipNormals();

    if (elem.property("face_normals").getBool())
        mesh.setupFaceNormalsAsVertexNormals();

    if (!elem.property("transform").getTransform().matrix().isIdentity())
        mesh.transform(elem.property("transform").getTransform());

    // Build bounding box
    BoundingBox bbox = BoundingBox::Empty();
    for (const auto& v : mesh.vertices)
        bbox.extend(v);
    bbox.inflate(1e-5f); // Make sure it has a volume

    const uint32 id = ctx.Shapes->addShape(name, Shape{ this, bbox });

    // Check if shape is actually just a simple plane
    auto plane = mesh.getAsPlane();
    if (plane.has_value())
        ctx.Shapes->addPlaneShape(id, plane.value());

    // Register triangle shape
    TriShape shape;
    shape.VertexCount = mesh.vertices.size();
    shape.NormalCount = mesh.normals.size();
    shape.TexCount    = mesh.texcoords.size();
    shape.FaceCount   = mesh.faceCount();
    shape.Area        = mesh.computeArea();
    shape.BvhID       = ctx.Database->Tables["trimesh_primbvh"].entryCount();
    ctx.Shapes->addTriShape(id, shape);

    IG_ASSERT(mesh.face_normals.size() == mesh.faceCount(), "Expected valid face normals!");
    IG_ASSERT((mesh.indices.size() % 4) == 0, "Expected index buffer count to be a multiple of 4!");

    // Export data:
    IG_LOG(L_DEBUG) << "Generating triangle mesh for shape " << name << std::endl;

    mDtbMutex.lock();
    auto& shapeTable = result.Database.Shapes;
    auto& meshData   = shapeTable.addLookup(this->id(), 0, DefaultAlignment);
    VectorSerializer meshSerializer(meshData, false);
    meshSerializer.write((uint32)mesh.faceCount());
    meshSerializer.write((uint32)mesh.vertices.size());
    meshSerializer.write((uint32)mesh.normals.size());
    meshSerializer.write((uint32)mesh.texcoords.size());
    meshSerializer.writeAligned(mesh.vertices, DefaultAlignment, true);
    meshSerializer.writeAligned(mesh.normals, DefaultAlignment, true);
    meshSerializer.writeAligned(mesh.face_normals, DefaultAlignment, true);
    meshSerializer.write(mesh.indices, true);   // Already aligned
    meshSerializer.write(mesh.texcoords, true); // Aligned to 4*2 bytes
    meshSerializer.write(mesh.face_inv_area, true);
    mDtbMutex.unlock();

    if (ctx.Target == Target::NVVM || ctx.Target == Target::AMDGPU) {
        setup_bvh<2, 1>(mesh, result.Database, mBvhMutex);
    } else if (ctx.Target == Target::GENERIC || ctx.Target == Target::SINGLE || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
        setup_bvh<4, 4>(mesh, result.Database, mBvhMutex);
    } else {
        setup_bvh<8, 4>(mesh, result.Database, mBvhMutex);
    }
}

std::string TriMeshProvider::generateShapeCode(const LoaderContext& ctx)
{
    IG_UNUSED(ctx);
    return "make_trimesh_shape(load_trimesh(data))";
}

std::string TriMeshProvider::generateTraversalCode(LoaderContext& ctx)
{
    std::stringstream stream;
    stream << "  let shapes = device.load_dyntable(\"trimesh_shapes\");";
    return stream.str();
}
} // namespace IG