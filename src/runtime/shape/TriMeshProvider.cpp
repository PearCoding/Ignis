#include "TriMeshProvider.h"
#include "Image.h"
#include "StringUtils.h"
#include "bvh/TriBVHAdapter.h"
#include "loader/LoaderShape.h"
#include "mesh/MtsSerializedFile.h"
#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"
#include "serialization/FileSerializer.h"
#include "serialization/VectorSerializer.h"
#include "shader/ShaderUtils.h"

#include "Logger.h"

#include <tbb/scalable_allocator.h>

namespace IG {

inline TriMesh setup_mesh_triangle(const SceneObject& elem)
{
    const Vector3f p0 = elem.property("p0").getVector3(Vector3f(0, 0, 0));
    const Vector3f p1 = elem.property("p1").getVector3(Vector3f(1, 0, 0));
    const Vector3f p2 = elem.property("p2").getVector3(Vector3f(0, 1, 0));
    return TriMesh::MakeTriangle(p0, p1, p2);
}

inline TriMesh setup_mesh_rectangle(const SceneObject& elem)
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

inline TriMesh setup_mesh_cube(const SceneObject& elem)
{
    const float width     = elem.property("width").getNumber(2.0f);
    const float height    = elem.property("height").getNumber(2.0f);
    const float depth     = elem.property("depth").getNumber(2.0f);
    const Vector3f origin = elem.property("origin").getVector3(Vector3f(-width / 2, -height / 2, -depth / 2));
    return TriMesh::MakeBox(origin, Vector3f::UnitX() * width, Vector3f::UnitY() * height, Vector3f::UnitZ() * depth);
}

inline TriMesh setup_mesh_ico_sphere(const SceneObject& elem)
{
    const Vector3f center     = elem.property("center").getVector3();
    const float radius        = elem.property("radius").getNumber(1.0f);
    const uint32 subdivisions = elem.property("subdivisions").getInteger(4);
    return TriMesh::MakeIcoSphere(center, radius, subdivisions);
}

inline TriMesh setup_mesh_uv_sphere(const SceneObject& elem)
{
    const Vector3f center = elem.property("center").getVector3();
    const float radius    = elem.property("radius").getNumber(1.0f);
    const uint32 stacks   = elem.property("stacks").getInteger(32);
    const uint32 slices   = elem.property("slices").getInteger(16);
    return TriMesh::MakeUVSphere(center, radius, stacks, slices);
}

inline TriMesh setup_mesh_cylinder(const SceneObject& elem)
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

inline TriMesh setup_mesh_cone(const SceneObject& elem)
{
    const Vector3f baseCenter = elem.property("p0").getVector3();
    const Vector3f tipCenter  = elem.property("p1").getVector3(Vector3f(0, 0, 1));
    const float radius        = elem.property("radius").getNumber(1.0f);
    const uint32 sections     = elem.property("sections").getInteger(32);
    const bool filled         = elem.property("filled").getBool(true);
    return TriMesh::MakeCone(baseCenter, radius, tipCenter, sections, filled);
}

inline TriMesh setup_mesh_disk(const SceneObject& elem)
{
    const Vector3f origin = elem.property("origin").getVector3();
    const Vector3f normal = elem.property("normal").getVector3(Vector3f(0, 0, 1));
    const float radius    = elem.property("radius").getNumber(1.0f);
    const uint32 sections = elem.property("sections").getInteger(32);
    return TriMesh::MakeDisk(origin, normal, radius, sections);
}

inline TriMesh setup_mesh_gauss(const SceneObject& elem)
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

inline TriMesh setup_mesh_gauss_lobe(const SceneObject& elem)
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

inline TriMesh setup_mesh_obj(const std::string& name, const SceneObject& elem, const LoaderContext& ctx)
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

inline TriMesh setup_mesh_ply(const std::string& name, const SceneObject& elem, const LoaderContext& ctx)
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

inline TriMesh setup_mesh_mitsuba(const std::string& name, const SceneObject& elem, const LoaderContext& ctx)
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

inline TriMesh setup_mesh_external(const std::string& name, const SceneObject& elem, const LoaderContext& ctx)
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

inline TriMesh setup_mesh_inline(const std::string& name, const SceneObject& elem, const LoaderContext&)
{
    auto propIndices   = elem.propertyOpt("indices");
    auto propVertices  = elem.propertyOpt("vertices");
    auto propNormals   = elem.propertyOpt("normals");
    auto propTexCoords = elem.propertyOpt("texcoords");

    bool hasIndices   = false;
    bool hasVertices  = false;
    bool hasNormals   = false;
    bool hasTexCoords = false;

    // TODO: Would be nice to acquire the arrays and remove data from the properties, as, very likely, they will not be needed anymore.
    // This however requires write access to Scene to consume properties
    SceneProperty::IntegerArray indices;
    if (propIndices.has_value())
        indices = propIndices.value().get().getIntegerArray(&hasIndices);
    SceneProperty::NumberArray vertices;
    if (propVertices.has_value())
        vertices = propVertices.value().get().getNumberArray(&hasVertices);
    SceneProperty::NumberArray normals;
    if (propNormals.has_value())
        normals = propNormals.value().get().getNumberArray(&hasNormals);
    SceneProperty::NumberArray texcoords;
    if (propTexCoords.has_value())
        texcoords = propTexCoords.value().get().getNumberArray(&hasTexCoords);

    if (!hasIndices) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': No indices given" << std::endl;
        return {};
    }

    if ((indices.size() % 3) != 0) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Number of indices not multiple of 3. Only triangular faces are accepted" << std::endl;
        return {};
    }

    if (!hasVertices) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': No vertices given" << std::endl;
        return {};
    }

    if ((vertices.size() % 3) != 0) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Number of vertices not multiple of 3" << std::endl;
        return {};
    }

    if (hasNormals && vertices.size() != normals.size()) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Number of normals does not match number of vertices" << std::endl;
        return {};
    }

    if (hasTexCoords && (texcoords.size() % 2) != 0) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Number of texcoords not multiple of 2" << std::endl;
        return {};
    }

    if (hasTexCoords && vertices.size() / 3 != texcoords.size() / 2) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Number of texcoords entries divided by 2 does not match number of vertices entries divided by 3" << std::endl;
        return {};
    }

    TriMesh mesh;

    // Copy data
    const size_t faceCount = indices.size() / 3;
    mesh.indices.resize(faceCount * 4);
    for (size_t i = 0; i < faceCount; ++i) {
        mesh.indices[4 * i + 0] = indices[3 * i + 0];
        mesh.indices[4 * i + 1] = indices[3 * i + 1];
        mesh.indices[4 * i + 2] = indices[3 * i + 2];
        mesh.indices[4 * i + 3] = 0;
    }

    const size_t pointCount = vertices.size() / 3;
    mesh.vertices.resize(pointCount);
    for (size_t i = 0; i < pointCount; ++i)
        mesh.vertices[i] = StVector3f(vertices[3 * i + 0], vertices[3 * i + 1], vertices[3 * i + 2]);

    if (hasNormals) {
        mesh.normals.resize(pointCount);
        for (size_t i = 0; i < pointCount; ++i)
            mesh.normals[i] = StVector3f(normals[3 * i + 0], normals[3 * i + 1], normals[3 * i + 2]);
    } else {
        mesh.computeVertexNormals();
    }

    if (hasTexCoords) {
        mesh.texcoords.resize(pointCount);
        for (size_t i = 0; i < pointCount; ++i)
            mesh.texcoords[i] = StVector2f(texcoords[2 * i + 0], texcoords[2 * i + 1]);
    } else {
        mesh.makeTexCoordsNormalized();
    }

    mesh.vertices.shrink_to_fit();
    mesh.normals.shrink_to_fit();
    mesh.texcoords.shrink_to_fit();
    mesh.indices.shrink_to_fit();

    return mesh;
}

template <size_t N, size_t T>
struct BvhTemporary {
    std::vector<typename BvhNTriM<N, T>::Node, tbb::scalable_allocator<typename BvhNTriM<N, T>::Node>> nodes;
    std::vector<typename BvhNTriM<N, T>::Tri, tbb::scalable_allocator<typename BvhNTriM<N, T>::Tri>> tris;
};

template <size_t N, size_t T>
static void serialize_bvh(Serializer& serializer, BvhTemporary<N, T>& bvh)
{
    uint32 node_count = (uint32)bvh.nodes.size();
    uint32 tri_count  = (uint32)bvh.tris.size();
    uint32 _pad       = 0;

    serializer | node_count;
    serializer | tri_count; // Not really needed, but just dump it out
    serializer | _pad;      // Padding
    serializer | _pad;      // Padding

    if (serializer.isReadMode()) {
        serializer.read(bvh.nodes, node_count);
        serializer.read(bvh.tris, tri_count);
    } else {
        serializer.write(bvh.nodes, true);
        serializer.write(bvh.tris, true);
    }
}

template <size_t N, size_t T>
static uint64 setup_bvh(const TriMesh& mesh, LoaderContext& ctx, const std::string& name, std::mutex& mutex)
{
    constexpr size_t MinFaceCountForCache = 500000;
    IG_ASSERT(mesh.faceCount() > 0, "Expected mesh to contain some triangles");

    const std::filesystem::path path = ctx.CacheManager->directory() / ("_bvh_" + name + ".bin");
    bool inCache                     = false;
    const bool isEligible            = mesh.faceCount() > MinFaceCountForCache; // Do not waste effort for small meshes
    if (isEligible && ctx.CacheManager->isEnabled()) {
        const std::string hash = mesh.computeHash();
        inCache                = ctx.CacheManager->checkAndUpdate("bvh_" + name, hash);
    }

    BvhTemporary<N, T> bvh;
    if (!inCache || !std::filesystem::exists(path)) {
        build_bvh<N, T>(mesh, bvh.nodes, bvh.tris);

        if (ctx.CacheManager->isEnabled() && isEligible) {
            FileSerializer serializer(path, false);
            serialize_bvh(serializer, bvh);
        }
    } else {
        FileSerializer serializer(path, true);
        serialize_bvh(serializer, bvh);
    }

    {
        std::lock_guard<std::mutex> _guard(mutex);
        auto& bvhTable = ctx.Database.FixTables["trimesh_primbvh"];
        auto& bvhData  = bvhTable.addEntry(DefaultAlignment);
        uint64 offset  = bvhTable.currentOffset() / sizeof(float);
        VectorSerializer serializer(bvhData, false);
        serialize_bvh(serializer, bvh);
        return offset;
    }
}

static void handleRefinement(TriMesh& mesh, const SceneObject& elem)
{
    const float min_area = elem.property("refinement").getNumber(0);
    if (min_area <= FltEps)
        return;

    constexpr size_t MaxIter = 16; // Just to prevent infinite iterations

    // Refine the mesh until sufficient or max iterations
    IG_LOG(L_DEBUG) << "Refining mesh until " << min_area << std::endl;
    for (size_t k = 0; k < MaxIter; ++k) {
        std::vector<bool> mask;
        mesh.markAreaGreater(mask, min_area);

        // Check if we still have to refine the mesh
        bool check = false;
        for (const bool b : mask) {
            if (b) {
                check = true;
                break;
            }
        }

        if (!check)
            break;

        mesh.subdivide(&mask);
    }
}

static void applyDisplacement(TriMesh& mesh, const Image& image, float amount)
{
    // Generate necessary attributes if necessary
    if (mesh.normals.size() != mesh.vertices.size())
        mesh.computeVertexNormals();

    if (mesh.texcoords.size() / 2 != mesh.vertices.size() / 3)
        mesh.makeTexCoordsNormalized();

    // Apply displacement
    IG_LOG(L_DEBUG) << "Applying displacement for mesh" << std::endl;
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        StVector3f& v       = mesh.vertices[i];
        const StVector3f nv = mesh.normals[i];
        const StVector2f tv = mesh.texcoords[i];

        const Vector4f val = image.eval(tv, Image::BorderMethod::Repeat, Image::FilterMethod::Nearest);
        const float dist   = val.block<3, 1>(0, 0).mean() * amount;
        v += nv * dist;
    }
}

static void handleDisplacement(TriMesh& mesh, const LoaderContext& ctx, const SceneObject& elem)
{
    const auto displacementProp = elem.property("displacement");
    if (!displacementProp.isValid() || displacementProp.type() != SceneProperty::PT_STRING)
        return;

    const auto filename = ctx.handlePath(displacementProp.getString(), elem);
    const Image image   = Image::load(filename);

    if (!image.isValid())
        return;

    const float amount = elem.property("displacement_amount").getNumber(1.0f);

    applyDisplacement(mesh, image, amount);
}

static void handleModification(TriMesh& mesh, const LoaderContext& ctx, const std::string& name, const SceneObject& elem)
{
    const size_t subdivisionCount  = elem.property("subdivision").getInteger(0);
    const float min_area           = elem.property("refinement").getNumber(0);
    const std::string displacement = elem.property("displacement").getString("");
    const float amount             = elem.property("displacement_amount").getNumber(1.0f);

    bool hasModification = subdivisionCount > 0 || min_area > 0 || !displacement.empty();
    if (!hasModification)
        return;

    const std::filesystem::path path = ctx.CacheManager->directory() / ("_shape_" + name + ".ply");

    bool inCache = false;
    if (ctx.CacheManager->isEnabled()) {
        const std::string hash = mesh.computeHash() + "_" + std::to_string(subdivisionCount) + "_" + std::to_string(min_area) + "_" + displacement + "_" + std::to_string(amount);
        inCache                = ctx.CacheManager->checkAndUpdate("shape_" + name, hash);
    }

    if (!inCache || !std::filesystem::exists(path)) {
        for (size_t i = 0; i < subdivisionCount; ++i)
            mesh.subdivide();

        handleRefinement(mesh, elem);
        handleDisplacement(mesh, ctx, elem);

        if (ctx.CacheManager->isEnabled())
            ply::save(mesh, path);
    } else {
        IG_LOG(L_DEBUG) << "Loading modified mesh '" << name << "' from cache " << path << std::endl;
        mesh = ply::load(path);
    }

    // Re-Evaluate normals if necessary
    if (elem.property("smooth_normals").getBool(false))
        mesh.computeVertexNormals();
    else
        mesh.setupFaceNormalsAsVertexNormals();
}

static inline std::pair<uint32, uint32> split_u64_to_u32(uint64 a)
{
    return { uint32(a & 0xFFFFFFFF), uint32((a >> 32) & 0xFFFFFFFF) };
}

void TriMeshProvider::handle(LoaderContext& ctx, ShapeMTAccessor& acc, const std::string& name, const SceneObject& elem)
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
    } else if (elem.pluginType() == "inline") {
        mesh = setup_mesh_inline(name, elem, ctx);
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
    else if (elem.property("smooth_normals").getBool(false))
        mesh.computeVertexNormals();

    if (!elem.property("transform").getTransform().matrix().isIdentity())
        mesh.transform(elem.property("transform").getTransform());

    handleModification(mesh, ctx, name, elem);

    // Build bounding box
    BoundingBox bbox = mesh.computeBBox();
    bbox.inflate(1e-5f); // Make sure it has a volume

    IG_ASSERT((mesh.indices.size() % 4) == 0, "Expected index buffer count to be a multiple of 4!");

    // Setup bvh
    uint64 bvh_offset = 0;
    if (ctx.Options.Target.isGPU()) {
        bvh_offset = setup_bvh<2, 1>(mesh, ctx, name, mBvhMutex);
    } else if (ctx.Options.Target.vectorWidth() < 8) {
        bvh_offset = setup_bvh<4, 4>(mesh, ctx, name, mBvhMutex);
    } else {
        bvh_offset = setup_bvh<8, 4>(mesh, ctx, name, mBvhMutex);
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
        stream << "  let prim_bvhs = make_gpu_trimesh_bvh_table(device);" << std::endl;
    } else {
        stream << "  let prim_bvhs = make_cpu_trimesh_bvh_table(device, " << ctx.Options.Target.vectorWidth() << ");" << std::endl;
    }

    stream << "  let trace   = TraceAccessor { shapes = trimesh_shapes, entities = entities };" << std::endl
           << "  let handler = device.get_traversal_handler_multiple(prim_bvhs);" << std::endl;
    return stream.str();
}
} // namespace IG