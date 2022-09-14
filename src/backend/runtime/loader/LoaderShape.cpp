#include "LoaderShape.h"
#include "Loader.h"

#include "Logger.h"
#include "shape/SphereProvider.h"
#include "shape/TriMeshProvider.h"

#include "serialization/VectorSerializer.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <sstream>

#include <tbb/parallel_for.h>
#include <tbb/scalable_allocator.h>

// Does not improve much, as it is IO bounded
#define IG_PARALLEL_LOAD_SHAPE
// There is no reason to not build multiple BVHs in parallel
#define IG_PARALLEL_BVH

namespace IG {

static const struct ShapeProviderEntry {
    const char* Name;
    const char* Provider;
} _providers[] = {
    { "triangle", "trimesh" },
    { "rectangle", "trimesh" },
    { "cube", "trimesh" },
    { "box", "trimesh" },
    { "sphere", "trimesh" }, // TODO
    { "icosphere", "trimesh" },
    { "uvsphere", "trimesh" },
    { "cylinder", "trimesh" },
    { "cone", "trimesh" },
    { "disk", "trimesh" },
    { "obj", "trimesh" },
    { "ply", "trimesh" },
    { "mitsuba", "trimesh" },
    { "external", "trimesh" },
    { "", nullptr }
};

static const ShapeProviderEntry* getShapeProviderEntry(const std::string& name)
{
    const std::string lower_name = to_lowercase(name);
    for (size_t i = 0; _providers[i].Provider; ++i) {
        if (_providers[i].Name == lower_name)
            return &_providers[i];
    }
    IG_LOG(L_ERROR) << "No shape type '" << name << "' available" << std::endl;
    return nullptr;
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

    // Check which shape provider we need
    for (const auto& obj : ctx.Scene.shapes()) {
        auto entry = getShapeProviderEntry(obj.second->pluginType());
        if (!entry)
            continue;

        auto it = ctx.ShapeProviders.find(entry->Provider);
        if (it == ctx.ShapeProviders.end()) {
            if (std::string_view(entry->Provider) == "trimesh") {
                ctx.ShapeProviders[entry->Provider] = std::make_unique<TriMeshProvider>();
            } else if (std::string_view(entry->Provider) == "sphere") {
                ctx.ShapeProviders[entry->Provider] = std::make_unique<SphereProvider>();
            } else {
                IG_ASSERT(false, "Shape provider entries and implementation is incomplete!");
            }
        }
    }

    // Preallocate mesh storage
    std::vector<TriMesh> meshes;
    meshes.resize(ctx.Scene.shapes().size());
    std::vector<BoundingBox> boxes;
    boxes.resize(ctx.Scene.shapes().size());

    std::mutex plane_shape_mutex;

    // TODO: Only use a single parallel call to handle everything including BVH etc for a single shape.
    // This should improvie memory usage while loading etc

    // Load meshes in parallel
    // This is not always useful as the bottleneck is provably the IO, but better trying...
    const auto load_shape = [&](size_t i) {
        const std::string name = ids.at(i);
        const auto child       = ctx.Scene.shape(name);

        auto entry = getShapeProviderEntry(child->pluginType());
        if (!entry)
            continue;

        auto it = ctx.ShapeProviders.find(entry->Provider);
        if (it == ctx.ShapeProviders.end())
            continue;

        it->second->handle(*child, ctx);
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
        } else if (child->pluginType() == "external") {
            mesh = setup_mesh_external(name, *child, ctx);
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

        if (!child->property("transform").getTransform().matrix().isIdentity())
            mesh.transform(child->property("transform").getTransform());

        // Check if shape is actually just a simple plane
        auto plane = mesh.getAsPlane();
        if (plane.has_value()) {
            plane_shape_mutex.lock();
            ctx.Environment.PlaneShapes[(uint32)i] = plane.value();
            plane_shape_mutex.unlock();
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

    // Provider specific -----------------------------------------------------------------------------
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
        shape.Area        = mesh.computeArea();
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
    } else if (ctx.Target == Target::GENERIC || ctx.Target == Target::SINGLE || ctx.Target == Target::ASIMD || ctx.Target == Target::SSE42) {
        setup_bvhs<4, 4>(meshes, result);
    } else {
        setup_bvhs<8, 4>(meshes, result);
    }
    // -----------------------------------------------------------------------------

    return true;
}
} // namespace IG