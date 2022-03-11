#include "LoaderShape.h"
#include "Loader.h"

#include "Logger.h"
#include "bvh/TriBVHAdapter.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

#include "serialization/VectorSerializer.h"

#include <algorithm>
#include <chrono>
#include <sstream>

#include <tbb/parallel_for.h>
#include <tbb/scalable_allocator.h>

// Does not improve much, as it is IO bounded
#define IG_PARALLEL_LOAD_SHAPE
// There is no reason to not build multiple BVHs in parallel
#define IG_PARALLEL_BVH

namespace IG {

using namespace Parser;

struct TransformCache {
    const Matrix4f TransformMatrix;
    const Matrix3f NormalMatrix;

    inline explicit TransformCache(const Transformf& t)
        : TransformMatrix(t.matrix())
        , NormalMatrix(TransformMatrix.block<3, 3>(0, 0).transpose().inverse())
    {
    }

    [[nodiscard]] inline Vector3f applyTransform(const Vector3f& v) const
    {
        Vector4f w = TransformMatrix * Vector4f(v(0), v(1), v(2), 1.0f);
        w /= w(3);
        return Vector3f(w(0), w(1), w(2));
    }

    [[nodiscard]] inline Vector3f applyNormal(const Vector3f& n) const
    {
        return (NormalMatrix * n).normalized();
    }
};

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
    const auto filename = ctx.handlePath(elem.property("filename").getString(), elem);
    // IG_LOG(L_DEBUG) << "Shape '" << name << "': Trying to load obj file " << filename << std::endl;
    auto trimesh = obj::load(filename);
    if (trimesh.vertices.empty()) {
        IG_LOG(L_WARNING) << "Shape '" << name << "': Can not load shape given by file " << filename << std::endl;
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
        IG_LOG(L_WARNING) << "Shape '" << name << "': Can not load shape given by file " << filename << std::endl;
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
        IG_LOG(L_WARNING) << "Shape '" << name << "': Can not load shape given by file " << filename << std::endl;
        return TriMesh();
    }
    return trimesh;
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
    IG_LOG(L_DEBUG) << "Building BVHs took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

    // Write non-parallel
    IG_LOG(L_DEBUG) << "Storing BVHs ..." << std::endl;
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
    IG_LOG(L_DEBUG) << "Storing BVHs took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start2).count() / 1000.0f << " seconds" << std::endl;
}

bool LoaderShape::load(LoaderContext& ctx, LoaderResult& result)
{
    // To make use of parallelization and workaround the map restrictions
    // we do have to construct a map
    std::vector<std::string> ids(ctx.Scene.shapes().size());
    std::transform(ctx.Scene.shapes().begin(), ctx.Scene.shapes().end(), ids.begin(),
                   [](const std::pair<std::string, std::shared_ptr<Object>>& pair) {
                       return pair.first;
                   });

    // Preallocate mesh storage
    std::vector<TriMesh> meshes;
    meshes.resize(ctx.Scene.shapes().size());
    std::vector<BoundingBox> boxes;
    boxes.resize(ctx.Scene.shapes().size());

    // Load meshes in parallel
    // This is not always useful as the bottleneck is provably the IO, but better trying...
    const auto load_mesh = [&](size_t i) {
        const std::string name = ids.at(i);
        const auto child       = ctx.Scene.shape(name);

        TriMesh& mesh = meshes[i];
        if (child->pluginType() == "triangle") {
            mesh = setup_mesh_triangle(*child);
        } else if (child->pluginType() == "rectangle") {
            mesh = setup_mesh_rectangle(*child);
        } else if (child->pluginType() == "cube" || child->pluginType() == "box") {
            mesh = setup_mesh_cube(*child);
        } else if (child->pluginType() == "sphere" || child->pluginType() == "icosphere") {
            mesh = setup_mesh_ico_sphere(*child);
        } else if (child->pluginType() == "uvsphere") {
            mesh = setup_mesh_uv_sphere(*child);
        } else if (child->pluginType() == "cylinder") {
            mesh = setup_mesh_cylinder(*child);
        } else if (child->pluginType() == "cone") {
            mesh = setup_mesh_cone(*child);
        } else if (child->pluginType() == "disk") {
            mesh = setup_mesh_disk(*child);
        } else if (child->pluginType() == "obj") {
            mesh = setup_mesh_obj(name, *child, ctx);
        } else if (child->pluginType() == "ply") {
            mesh = setup_mesh_ply(name, *child, ctx);
        } else if (child->pluginType() == "mitsuba") {
            mesh = setup_mesh_mitsuba(name, *child, ctx);
        } else {
            IG_LOG(L_ERROR) << "Shape '" << name << "': Can not load shape type '" << child->pluginType() << "'" << std::endl;
            return;
        }

        if (mesh.vertices.empty()) {
            IG_LOG(L_ERROR) << "Shape '" << name << "': While loading shape type '" << child->pluginType() << "' no vertices were generated" << std::endl;
            return;
        }

        if (mesh.faceCount() == 0) {
            IG_LOG(L_ERROR) << "Shape '" << name << "': While loading shape type '" << child->pluginType() << "' no indices were generated" << std::endl;
            return;
        }

        if (child->property("flip_normals").getBool())
            mesh.flipNormals();

        if (child->property("face_normals").getBool())
            mesh.setupFaceNormalsAsVertexNormals();

        TransformCache transform = TransformCache(child->property("transform").getTransform());
        if (!transform.TransformMatrix.isIdentity()) {
            for (auto& v : mesh.vertices)
                v = transform.applyTransform(v);
            for (auto& n : mesh.normals)
                n = transform.applyNormal(n);
            for (auto& n : mesh.face_normals)
                n = transform.applyNormal(n);
        }

        // Build bounding box
        BoundingBox& bbox = boxes[i];
        bbox              = BoundingBox::Empty();
        for (const auto& v : mesh.vertices)
            bbox.extend(v);
        bbox.inflate(1e-5f); // Make sure it has a volume
    };

    // Start loading!
    IG_LOG(L_DEBUG) << "Loading shapes..." << std::endl;
    const auto start1 = std::chrono::high_resolution_clock::now();
#ifdef IG_PARALLEL_LOAD_SHAPE
    tbb::parallel_for(tbb::blocked_range<size_t>(0, meshes.size()),
                      [&](const tbb::blocked_range<size_t>& range) {
                          for (size_t i = range.begin(); i != range.end(); ++i)
                              load_mesh(i);
                      });
#else
    for (size_t i = 0; i < meshes.size(); ++i)
        load_mesh(i);
#endif
    IG_LOG(L_DEBUG) << "Loading of shapes took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start1).count() / 1000.0f << " seconds" << std::endl;

    // Write non-parallel
    IG_LOG(L_DEBUG) << "Storing triangle meshes..." << std::endl;
    size_t counter    = 0;
    const auto start2 = std::chrono::high_resolution_clock::now();
    for (const auto& pair : ctx.Scene.shapes()) {
        const size_t id     = counter++;
        const TriMesh& mesh = meshes.at(id);

        // Register shape into environment
        Shape shape;
        shape.VertexCount = mesh.vertices.size();
        shape.NormalCount = mesh.normals.size();
        shape.TexCount    = mesh.texcoords.size();
        shape.FaceCount   = mesh.faceCount();
        shape.BoundingBox = boxes.at(id);

        const uint32 shapeID = (uint32)ctx.Environment.Shapes.size();
        ctx.Environment.Shapes.push_back(shape);
        ctx.Environment.ShapeIDs[pair.first] = shapeID;

        IG_ASSERT(mesh.face_normals.size() == mesh.faceCount(), "Expected valid face normals!");
        IG_ASSERT((mesh.indices.size() % 4) == 0, "Expected index buffer count to be a multiple of 4!");

        // Export data:
        IG_LOG(L_DEBUG) << "Generating triangle mesh for shape " << pair.first << std::endl;

        auto& meshData = result.Database.ShapeTable.addLookup(0, 0, DefaultAlignment); // TODO: No use of the typeid currently
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
    }
    IG_LOG(L_DEBUG) << "Storing of shapes took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start2).count() / 1000.0f << " seconds" << std::endl;

    if (ctx.Target == Target::NVVM || ctx.Target == Target::AMDGPU) {
        setup_bvhs<2, 1>(meshes, result);
    } else if (ctx.Target == Target::GENERIC || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
        setup_bvhs<4, 4>(meshes, result);
    } else {
        setup_bvhs<8, 4>(meshes, result);
    }

    return true;
}
} // namespace IG