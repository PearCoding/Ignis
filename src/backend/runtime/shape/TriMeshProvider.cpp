#include "TriMeshProvider.h"

#include "bvh/TriBVHAdapter.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

#include "Logger.h"

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
static void setup_bvhs(const std::vector<TriMesh>& meshes, LoaderResult& result)
{
    // Preload map entries
    std::vector<BvhTemporary<N, T>> bvhs;
    bvhs.resize(meshes.size());

    const auto build_mesh = [&](size_t id) {
        BvhTemporary<N, T>& tmp = bvhs[id];
        const TriMesh& mesh     = meshes.at(id);
        if (mesh.faceCount() > 0)
            build_bvh<N, T>(mesh, tmp.nodes, tmp.tris);
    };

    // Start building!
    IG_LOG(L_DEBUG) << "Building BVHs ..." << std::endl;
    const auto start1 = std::chrono::high_resolution_clock::now();
#ifdef IG_PARALLEL_BVH
    tbb::parallel_for(tbb::blocked_range<size_t>(0, meshes.size()),
                      [&](const tbb::blocked_range<size_t>& range) {
                          for (size_t i = range.begin(); i != range.end(); ++i)
                              build_mesh(i);
                      });
#else
    for (size_t i = 0; i < meshes.size(); ++i)
        build_mesh(i);
#endif
    IG_LOG(L_DEBUG) << "Building trimesh BVHs took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

    // Write non-parallel
    IG_LOG(L_DEBUG) << "Storing trimesh BVHs ..." << std::endl;
    const auto start2 = std::chrono::high_resolution_clock::now();
    for (const auto& bvh : bvhs) {
        auto& bvhData = result.Database.BVHTable.addLookup(0, 0, DefaultAlignment);
        VectorSerializer serializer(bvhData, false);
        serializer.write((uint32)bvh.nodes.size());
        serializer.write((uint32)bvh.tris.size()); // Not really needed, but just dump it out
        serializer.write((uint32)0);               // Padding
        serializer.write((uint32)0);               // Padding
        serializer.write(bvh.nodes, true);
        serializer.write(bvh.tris, true);
    }
    IG_LOG(L_DEBUG) << "Storing trimesh BVHs took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start2).count() / 1000.0f << " seconds" << std::endl;
}

void TriMeshProvider::handle(LoaderContext& ctx, const Parser::Object& elem)
{
}

void TriMeshProvider::setup(LoaderContext& ctx, LoaderResult& result)
{
}

std::string TriMeshProvider::generateTraversalCode(LoaderContext& ctx)
{
}
} // namespace IG