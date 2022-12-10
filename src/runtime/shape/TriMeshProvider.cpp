#include "TriMeshProvider.h"
#include "StringUtils.h"
#include "bvh/TriBVHAdapter.h"
#include "loader/LoaderShape.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"
#include "serialization/VectorSerializer.h"
#include "shader/ShaderUtils.h"

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

inline TriMesh setup_mesh_gauss(const Object& elem)
{
    // TODO: Add covariance based approach as well
    const Vector3f origin    = elem.property("origin").getVector3();
    const Vector3f normal    = elem.property("normal").getVector3(Vector3f(0, 0, 1));
    const float sigma        = elem.property("sigma").getNumber(1.0f);
    const float radius_scale = elem.property("radius_scale").getNumber(1.0f);
    const float height       = elem.property("height").getNumber(1.0f);
    const uint32 sections    = elem.property("sections").getInteger(32);
    const uint32 slices      = elem.property("slices").getInteger(16);
    return TriMesh::MakeRadialGaussian(origin, normal * height, sigma, radius_scale, sections, slices);
}

inline TriMesh setup_mesh_gauss_lobe(const Object& elem)
{
    const Vector3f origin    = elem.property("origin").getVector3();
    const Vector3f direction = elem.property("direction").getVector3(Vector3f(0, 0, 1));
    const float sigma_theta  = elem.property("sigma_theta").getNumber(1);
    const float sigma_phi    = elem.property("sigma_phi").getNumber(1);
    const float anisotropy   = elem.property("anisotropy").getNumber(0);
    const uint32 theta_size  = elem.property("theta_size").getInteger(64);
    const uint32 phi_size    = elem.property("phi_size").getInteger(128);
    const float scale        = elem.property("scale").getNumber(1);
    const Vector3f xAxis     = elem.property("x_axis").getVector3(Vector3f::UnitX());
    const Vector3f yAxis     = elem.property("y_axis").getVector3(Vector3f::UnitY());

    Matrix2f cov;
    cov << sigma_theta * sigma_theta, anisotropy * sigma_theta * sigma_phi,
        anisotropy * sigma_theta * sigma_phi, sigma_phi * sigma_phi;

    return TriMesh::MakeGaussianLobe(origin, direction, xAxis, yAxis, cov, theta_size, phi_size, scale);
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
static uint64 setup_bvh(const TriMesh& mesh, SceneDatabase& dtb, std::mutex& mutex)
{
    IG_ASSERT(mesh.faceCount() > 0, "Expected mesh to contain some triangles");

    BvhTemporary<N, T> bvh;
    build_bvh<N, T>(mesh, bvh.nodes, bvh.tris);

    mutex.lock();
    auto& bvhTable = dtb.FixTables["trimesh_primbvh"];
    auto& bvhData  = bvhTable.addEntry(DefaultAlignment);
    uint64 offset  = bvhTable.currentOffset() / sizeof(float);
    VectorSerializer serializer(bvhData, false);
    serializer.write((uint32)bvh.nodes.size());
    serializer.write((uint32)bvh.tris.size()); // Not really needed, but just dump it out
    serializer.write((uint32)0);               // Padding
    serializer.write((uint32)0);               // Padding
    serializer.write(bvh.nodes, true);
    serializer.write(bvh.tris, true);
    mutex.unlock();

    return offset;
}

static inline std::pair<uint32, uint32> split_u64_to_u32(uint64 a)
{
    return { uint32(a & 0xFFFFFFFF), uint32((a >> 32) & 0xFFFFFFFF) };
}

void TriMeshProvider::handle(LoaderContext& ctx, ShapeMTAccessor& acc, const std::string& name, const Parser::Object& elem)
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
    } else if (elem.pluginType() == "gauss") {
        mesh = setup_mesh_gauss(elem);
    } else if (elem.pluginType() == "gauss_lobe") {
        mesh = setup_mesh_gauss_lobe(elem);
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
    if (elem.property("flip_normals").getBool(false))
        mesh.flipNormals();

    if (elem.property("face_normals").getBool(false))
        mesh.setupFaceNormalsAsVertexNormals();

    if (!elem.property("transform").getTransform().matrix().isIdentity())
        mesh.transform(elem.property("transform").getTransform());

    // Build bounding box
    BoundingBox bbox = mesh.computeBBox();
    bbox.inflate(1e-5f); // Make sure it has a volume

    IG_ASSERT((mesh.indices.size() % 4) == 0, "Expected index buffer count to be a multiple of 4!");

    // Setup bvh
    uint64 bvh_offset = 0;
    if (ctx.Options.Target.isGPU()) {
        bvh_offset = setup_bvh<2, 1>(mesh, ctx.Database, mBvhMutex);
    } else if (ctx.Options.Target.vectorWidth() < 8) {
        bvh_offset = setup_bvh<4, 4>(mesh, ctx.Database, mBvhMutex);
    } else {
        bvh_offset = setup_bvh<8, 4>(mesh, ctx.Database, mBvhMutex);
    }

    // Precompute approximative shapes outside the lock region
    const std::optional<PlaneShape> plane   = mesh.getAsPlane();
    const std::optional<SphereShape> sphere = plane.has_value() ? std::nullopt : mesh.getAsSphere();

    // Setup internal shape object
    TriShape trishape;
    trishape.VertexCount = mesh.vertices.size();
    trishape.NormalCount = mesh.normals.size();
    trishape.TexCount    = mesh.texcoords.size();
    trishape.FaceCount   = mesh.faceCount();
    trishape.Area        = mesh.computeArea();

    // Make sure the id used in shape is same as in the dyntable later
    acc.DatabaseAccessMutex.lock();
    IG_LOG(L_DEBUG) << "Generating triangle mesh for shape " << name << std::endl;

    auto& table         = ctx.Database.DynTables["shapes"];
    auto& meshData      = table.addLookup((uint32)this->id(), 0, DefaultAlignment);
    const size_t offset = table.currentOffset();

    VectorSerializer meshSerializer(meshData, false);
    // Header
    meshSerializer.write((uint32)mesh.faceCount());
    meshSerializer.write((uint32)mesh.vertices.size());
    meshSerializer.write((uint32)mesh.normals.size());
    meshSerializer.write((uint32)mesh.texcoords.size());

    // Local bounding box
    meshSerializer.write(bbox.min);
    meshSerializer.write((float)0);
    meshSerializer.write(bbox.max);
    meshSerializer.write((float)0);

    // Data
    meshSerializer.writeAligned(mesh.vertices, DefaultAlignment, true);
    meshSerializer.writeAligned(mesh.normals, DefaultAlignment, true);
    meshSerializer.write(mesh.indices, true);   // Already aligned
    meshSerializer.write(mesh.texcoords, true); // Aligned to 4*2 bytes

    const auto off  = split_u64_to_u32(bvh_offset);
    const uint32 id = ctx.Shapes->addShape(name, Shape{ this, (int32)off.first, (int32)off.second, 0, bbox, offset });
    IG_ASSERT(id + 1 == table.entryCount(), "Expected id to be in sync with dyntable entry count");

    // Check if shape is actually just a simple plane
    if (plane.has_value()) {
        ctx.Shapes->addPlaneShape(id, plane.value());
    } else {
        // If not a plane, it might be a simple sphere
        if (sphere.has_value())
            ctx.Shapes->addSphereShape(id, sphere.value());
    }

    // Add internal shape structure to table for potential area light usage
    ctx.Shapes->addTriShape(id, trishape);

    acc.DatabaseAccessMutex.unlock();
}

std::string TriMeshProvider::generateShapeCode(const LoaderContext& ctx)
{
    IG_UNUSED(ctx);
    return "make_trimesh_shape(load_trimesh(data))";
}

std::string TriMeshProvider::generateTraversalCode(const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << ShaderUtils::generateShapeLookup("trimesh_shapes", this, ctx) << std::endl;

    if (ctx.Options.Target.isGPU()) {
        stream << "  let prim_bvhs = make_gpu_trimesh_bvh_table(device, " << (ctx.Options.Target.gpuVendor() == GPUVendor::Nvidia ? "true" : "false") << ");" << std::endl;
    } else {
        stream << "  let prim_bvhs = make_cpu_trimesh_bvh_table(device, " << ctx.Options.Target.vectorWidth() << ");" << std::endl;
    }

    stream << "  let trace   = TraceAccessor { shapes = trimesh_shapes, entities = entities };" << std::endl
           << "  let handler = device.get_traversal_handler_multiple(prim_bvhs);" << std::endl;
    return stream.str();
}
} // namespace IG