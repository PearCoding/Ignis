#include "ObjFile.h"
#include "Logger.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <functional>
#include <tuple>

namespace IG::obj {

using ObjIndex = std::tuple<uint32, uint32, uint32>;

struct ObjIndexHash : public std::unary_function<ObjIndex, std::size_t> {
    std::size_t operator()(const ObjIndex& k) const
    {
        const size_t h0 = std::hash<uint32>{}(std::get<0>(k));
        const size_t h1 = std::hash<uint32>{}(std::get<1>(k));
        const size_t h2 = std::hash<uint32>{}(std::get<2>(k));
        return h0 ^ (h1 << 1) ^ (h2 << 2);
    }
};

TriMesh load(const std::filesystem::path& path, const std::optional<size_t>& shape_index)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.generic_u8string().c_str());

    if (!warn.empty())
        IG_LOG(L_WARNING) << "ObjFile " << path << ": " << warn << std::endl;

    if (!err.empty())
        IG_LOG(L_ERROR) << "ObjFile " << path << ": " << err << std::endl;

    if (!ret)
        return TriMesh{};

    if (attrib.vertices.empty()) {
        IG_LOG(L_ERROR) << "ObjFile " << path << ": No vertices given!" << std::endl;
        return TriMesh{};
    }

    if (shape_index.value_or(0) > shapes.size()) {
        IG_LOG(L_ERROR) << "ObjFile " << path << ": Contains multiple shapes but given shape index " << shape_index.value_or(0) << " is too large " << std::endl;
        return TriMesh{};
    }

    size_t indices_count = 0;
    if (shape_index.has_value()) {
        indices_count = shapes[shape_index.value()].mesh.indices.size();
    } else {
        for (const auto& shape : shapes)
            indices_count += shape.mesh.indices.size();
    }

    // Check if normals are used at all
    bool has_norms = false;
    if (!attrib.normals.empty()) {
        if (shape_index.has_value()) {
            for (const auto& ind : shapes[shape_index.value()].mesh.indices) {
                if (ind.normal_index >= 0) {
                    has_norms = true;
                    break;
                }
            }
        } else {
            for (const auto& shape : shapes) {
                for (const auto& ind : shape.mesh.indices) {
                    if (ind.normal_index >= 0) {
                        has_norms = true;
                        break;
                    }
                }
                if (has_norms)
                    break;
            }
        }
    }

    // Check if texcoords are used at all
    bool has_tex = false;
    if (!attrib.texcoords.empty()) {
        if (shape_index.has_value()) {
            for (const auto& ind : shapes[shape_index.value()].mesh.indices) {
                if (ind.texcoord_index >= 0) {
                    has_tex = true;
                    break;
                }
            }
        } else {
            for (const auto& shape : shapes) {
                for (const auto& ind : shape.mesh.indices) {
                    if (ind.texcoord_index >= 0) {
                        has_tex = true;
                        break;
                    }
                }
                if (has_tex)
                    break;
            }
        }
    }

    // Reserve memory
    TriMesh tri_mesh;
    tri_mesh.indices.reserve(indices_count * 4);
    tri_mesh.vertices.reserve(indices_count * 3);
    if (has_norms)
        tri_mesh.normals.reserve(indices_count * 3);
    if (has_tex)
        tri_mesh.texcoords.reserve(indices_count * 3);

    std::unordered_map<ObjIndex, uint32, ObjIndexHash> index_map;
    index_map.reserve(indices_count * 3);

    // Add index to list if not already registred
    auto embed_index = [&](int vi, int ni, int ti) -> std::tuple<bool, uint32> {
        const auto t = ObjIndex{ vi, ni, ti };

        const auto it = index_map.find(t);
        if (it == index_map.end()) {
            uint32 id    = index_map.size();
            index_map[t] = id;
            return std::make_tuple(true, id);
        } else {
            return std::make_tuple(false, it->second);
        }
    };

    // Add vertex, normal and texcoord to buffer
    auto add_vnt = [&](int vi, int ni, int ti) -> void {
        tri_mesh.vertices.emplace_back(attrib.vertices[3 * vi + 0], attrib.vertices[3 * vi + 1], attrib.vertices[3 * vi + 2]);

        if (has_norms) {
            if (IG_LIKELY(ni >= 0))
                tri_mesh.normals.emplace_back(attrib.normals[3 * ni + 0], attrib.normals[3 * ni + 1], attrib.normals[3 * ni + 2]);
            else
                tri_mesh.normals.emplace_back(0, 0, 1); // TODO: Maybe fix with a follow-up pass?
        }

        if (has_tex) {
            if (IG_LIKELY(ti >= 0))
                tri_mesh.texcoords.emplace_back(attrib.texcoords[2 * ti + 0], attrib.texcoords[2 * ti + 1]);
            else
                tri_mesh.texcoords.emplace_back(0, 0);
        }
    };

    // Add whole shape
    auto add_shape = [&](const tinyobj::shape_t& shape) -> void {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            [[maybe_unused]] int fv = shape.mesh.num_face_vertices[f];
            IG_ASSERT(fv == 3, "Expected tinyobjloader generating triangular data!");

            for (int e = 0; e < 3 /*fv*/; ++e) {
                const auto& p = shape.mesh.indices[index_offset + e];

                const auto& r = embed_index(p.vertex_index, p.normal_index, p.texcoord_index);
                tri_mesh.indices.push_back(std::get<1>(r));
                if (std::get<0>(r))
                    add_vnt(p.vertex_index, p.normal_index, p.texcoord_index);
            }

            tri_mesh.indices.push_back(shape.mesh.material_ids[f]); // Last entry is material index (Not supported anymore)
            index_offset += 3 /*fv*/;
        }
    };

    // Depending on user parameter, add a specific shape or all shapes to trimesh
    if (shape_index.has_value()) {
        add_shape(shapes[shape_index.value()]);
    } else {
        for (const auto& shape : shapes)
            add_shape(shape);
    }

    // Cleanup
    // TODO: This does not work due to fp precision problems
    // const size_t removedBadAreas = tri_mesh.removeZeroAreaTriangles();
    // if (removedBadAreas != 0)
    //     IG_LOG(L_WARNING) << "ObjFile " << path << ": Removed " << removedBadAreas << " triangles with zero area" << std::endl;

    // Normals
    bool hasBadAreas = false;
    tri_mesh.computeFaceNormals(&hasBadAreas);
    // if (hasBadAreas)
    //     IG_LOG(L_WARNING) << "ObjFile " << path << ": Triangle mesh contains triangles with zero area" << std::endl;

    if (!has_norms) {
        IG_LOG(L_WARNING) << "ObjFile " << path << ": No valid normals given. Recalculating " << std::endl;
        tri_mesh.computeVertexNormals();
    }

    // Texcoords
    if (!has_tex) {
        IG_LOG(L_WARNING) << "ObjFile " << path << ": No texture coordinates are present, using default value." << std::endl;
        tri_mesh.makeTexCoordsZero();
    }

    tri_mesh.vertices.shrink_to_fit();
    tri_mesh.normals.shrink_to_fit();
    tri_mesh.texcoords.shrink_to_fit();
    tri_mesh.indices.shrink_to_fit();

    return tri_mesh;
}
} // namespace IG::obj
