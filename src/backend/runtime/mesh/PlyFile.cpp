#include "PlyFile.h"
#include "Logger.h"
#include "Triangulation.h"

#include <climits>
#include <fstream>
#include <sstream>

namespace IG {
// https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
template <typename T>
T swap_endian(T u)
{
    static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

    union {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

namespace ply {
static std::vector<uint32_t> triangulatePly(const std::filesystem::path& path,
                                            const std::vector<Vector3f>& vertices, const std::vector<uint32_t>& glb_indices,
                                            bool& warned)
{
    if (vertices.size() < 3) {
        return {};
    } else if (vertices.size() == 3) {
        std::vector<uint32_t> convex_inds;
        convex_inds.insert(convex_inds.end(), { glb_indices[0], glb_indices[1], glb_indices[2] });
        return convex_inds;
    } else if (vertices.size() == 4) {
        std::vector<uint32_t> convex_inds;
        convex_inds.insert(convex_inds.end(), { glb_indices[0], glb_indices[1], glb_indices[2] });
        convex_inds.insert(convex_inds.end(), { glb_indices[0], glb_indices[2], glb_indices[3] });
        return convex_inds;
    } else {
        std::vector<int> inds = Triangulation::triangulate(vertices);
        if (!inds.empty()) {
            std::vector<uint32_t> res_indices(inds.size());
            std::transform(inds.begin(), inds.end(), res_indices.begin(), [&](int i) { return glb_indices[i]; });
            return res_indices;
        }

        if (!warned) {
            // Could not triangulate, lets just convex triangulate and give up
            IG_LOG(L_WARNING) << "PlyFile " << path << ": Given polygonal face is malformed, approximating with convex triangulation" << std::endl;
            warned = true; // Just warn once
        }

        std::vector<uint32_t> convex_inds;
        convex_inds.insert(convex_inds.end(), { glb_indices[0], glb_indices[1], glb_indices[2] });
        for (uint32_t j = 3; j < (uint32_t)vertices.size(); ++j) {
            uint32_t prev = j - 1;
            uint32_t next = j;
            convex_inds.insert(convex_inds.end(), { glb_indices[0], glb_indices[prev], glb_indices[next] });
        }
        return convex_inds;
    }
}

struct Header {
    int VertexCount       = 0;
    int FaceCount         = 0;
    int XElem             = -1;
    int YElem             = -1;
    int ZElem             = -1;
    int NXElem            = -1;
    int NYElem            = -1;
    int NZElem            = -1;
    int UElem             = -1;
    int VElem             = -1;
    int VertexPropCount   = 0;
    int IndElem           = -1;
    bool SwitchEndianness = false;

    inline bool hasVertices() const { return XElem >= 0 && YElem >= 0 && ZElem >= 0; }
    inline bool hasNormals() const { return NXElem >= 0 && NYElem >= 0 && NZElem >= 0; }
    inline bool hasUVs() const { return UElem >= 0 && VElem >= 0; }
    inline bool hasIndices() const { return IndElem >= 0; }
};

static TriMesh read(const std::filesystem::path& path, std::istream& stream, const Header& header, bool ascii)
{
    const auto readFloat = [&]() {
        float val;
        stream.read(reinterpret_cast<char*>(&val), sizeof(val));
        if (header.SwitchEndianness)
            val = swap_endian<float>(val);
        return val;
    };

    const auto readIdx = [&]() {
        uint32_t val;
        stream.read(reinterpret_cast<char*>(&val), sizeof(val));
        if (header.SwitchEndianness)
            val = swap_endian<uint32_t>(val);
        return val;
    };

    TriMesh trimesh;
    trimesh.vertices.reserve(header.VertexCount);
    if (header.hasNormals())
        trimesh.normals.reserve(header.VertexCount);
    if (header.hasUVs())
        trimesh.texcoords.reserve(header.VertexCount);

    for (int i = 0; i < header.VertexCount; ++i) {

        float x = 0, y = 0, z = 0;
        float nx = 0, ny = 0, nz = 0;
        float u = 0, v = 0;

        if (ascii) {
            std::string line;
            if (!std::getline(stream, line)) {
                IG_LOG(L_ERROR) << "PlyFile " << path << ": Not enough vertices given" << std::endl;
                return TriMesh{}; // Failed
            }

            std::stringstream sstream(line);
            int elem = 0;
            while (sstream) {
                float val;
                sstream >> val;

                if (header.XElem == elem)
                    x = val;
                else if (header.YElem == elem)
                    y = val;
                else if (header.ZElem == elem)
                    z = val;
                else if (header.NXElem == elem)
                    nx = val;
                else if (header.NYElem == elem)
                    ny = val;
                else if (header.NZElem == elem)
                    nz = val;
                else if (header.UElem == elem)
                    u = val;
                else if (header.VElem == elem)
                    v = val;
                ++elem;
            }
        } else {
            for (int elem = 0; elem < header.VertexPropCount; ++elem) {
                float val = readFloat();
                if (header.XElem == elem)
                    x = val;
                else if (header.YElem == elem)
                    y = val;
                else if (header.ZElem == elem)
                    z = val;
                else if (header.NXElem == elem)
                    nx = val;
                else if (header.NYElem == elem)
                    ny = val;
                else if (header.NZElem == elem)
                    nz = val;
                else if (header.UElem == elem)
                    u = val;
                else if (header.VElem == elem)
                    v = val;
            }
        }

        trimesh.vertices.emplace_back(x, y, z);

        if (header.hasNormals()) {
            float norm = sqrt(nx * nx + ny * ny + nz * nz);
            if (norm == 0.0f)
                norm = 1.0f;
            trimesh.normals.emplace_back(nx / norm, ny / norm, nz / norm);
        }

        if (header.hasUVs()) {
            trimesh.texcoords.emplace_back(u, v);
        }
    }

    if (trimesh.vertices.empty()) {
        IG_LOG(L_ERROR) << "PlyFile " << path << ": No vertices found in ply file" << std::endl;
        return TriMesh{}; // Failed
    }

    trimesh.indices.reserve(header.FaceCount * 4);

    std::vector<uint32_t> tmp_indices;
    tmp_indices.reserve(3);
    std::vector<Vector3f> tmp_vertices;
    tmp_vertices.reserve(3);

    bool warned = false;
    if (ascii) {
        for (int i = 0; i < header.FaceCount; ++i) {
            std::string line;
            if (!std::getline(stream, line)) {
                IG_LOG(L_ERROR) << "PlyFile " << path << ": Not enough indices given" << std::endl;
                return TriMesh{}; // Failed
            }

            std::stringstream sstream(line);

            uint32_t elems;
            sstream >> elems;
            tmp_indices.resize(elems);
            tmp_vertices.resize(elems);

            for (uint32_t elem = 0; elem < elems; ++elem) {
                uint32_t index;
                sstream >> index;
                tmp_indices[elem]  = index;
                tmp_vertices[elem] = trimesh.vertices[index];
            }

            std::vector<uint32_t> inds = triangulatePly(path, tmp_vertices, tmp_indices, warned);

            for (size_t f = 0; f < inds.size() / 3; ++f) {
                trimesh.indices.insert(trimesh.indices.end(), { inds[f * 3 + 0], inds[f * 3 + 1], inds[f * 3 + 2], 0 });
            }
        }
    } else {
        for (int i = 0; i < header.FaceCount; ++i) {
            uint8_t elems;
            stream.read(reinterpret_cast<char*>(&elems), sizeof(elems));

            tmp_indices.resize(elems);
            tmp_vertices.resize(elems);

            for (uint32_t elem = 0; elem < elems; ++elem) {
                uint32_t index     = readIdx();
                tmp_indices[elem]  = index;
                tmp_vertices[elem] = trimesh.vertices[index];
            }

            std::vector<uint32_t> inds = triangulatePly(path, tmp_vertices, tmp_indices, warned);

            for (size_t f = 0; f < inds.size() / 3; ++f) {
                trimesh.indices.insert(trimesh.indices.end(), { inds[f * 3 + 0], inds[f * 3 + 1], inds[f * 3 + 2], 0 });
            }
        }
    }

    return trimesh;
}

static inline bool isAllowedVertIndType(const std::string& str)
{
    return str == "uchar"
           || str == "int"
           || str == "uint8_t"
           || str == "uint";
}

TriMesh load(const std::filesystem::path& path)
{
    std::fstream stream(path, std::ios::in | std::ios::binary);
    if (!stream) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' can not be opened." << std::endl;
        return TriMesh{};
    }

    // Header
    std::string magic;
    stream >> magic;
    if (magic != "ply") {
        IG_LOG(L_ERROR) << "Given file '" << path << "' is not a ply file." << std::endl;
        return TriMesh{};
    }

    std::string method;
    Header header;

    int facePropCounter = 0;
    for (std::string line; std::getline(stream, line);) {
        std::stringstream sstream(line);

        std::string action;
        sstream >> action;
        if (action == "comment")
            continue;
        else if (action == "format") {
            sstream >> method;
        } else if (action == "element") {
            std::string type;
            sstream >> type;
            if (type == "vertex")
                sstream >> header.VertexCount;
            else if (type == "face")
                sstream >> header.FaceCount;
        } else if (action == "property") {
            std::string type;
            sstream >> type;
            if (type == "float") {
                std::string name;
                sstream >> name;
                if (name == "x")
                    header.XElem = header.VertexPropCount;
                else if (name == "y")
                    header.YElem = header.VertexPropCount;
                else if (name == "z")
                    header.ZElem = header.VertexPropCount;
                else if (name == "nx")
                    header.NXElem = header.VertexPropCount;
                else if (name == "ny")
                    header.NYElem = header.VertexPropCount;
                else if (name == "nz")
                    header.NZElem = header.VertexPropCount;
                else if (name == "u" || name == "s")
                    header.UElem = header.VertexPropCount;
                else if (name == "v" || name == "t")
                    header.VElem = header.VertexPropCount;
                ++header.VertexPropCount;
            } else if (type == "list") {
                ++facePropCounter;

                std::string countType;
                sstream >> countType;

                std::string indType;
                sstream >> indType;

                std::string name;
                sstream >> name;
                if (!isAllowedVertIndType(countType)) {
                    IG_LOG(L_WARNING) << "PlyFile " << path << ": Only 'property list uchar int' is supported" << std::endl;
                    continue;
                }

                if (name == "vertex_indices" || name == "vertex_index")
                    header.IndElem = facePropCounter - 1;
            } else {
                IG_LOG(L_WARNING) << "PlyFile " << path << ": Only float or list properties allowed. Ignoring..." << std::endl;
                ++header.VertexPropCount;
            }
        } else if (action == "end_header")
            break;
    }

    // Content
    if (!header.hasVertices() || !header.hasIndices() || header.VertexCount <= 0 || header.FaceCount <= 0) {
        IG_LOG(L_WARNING) << "Ply file '" << path << "' does not contain valid mesh data" << std::endl;
        return TriMesh{};
    }

    header.SwitchEndianness = (method == "binary_big_endian");
    TriMesh trimesh         = read(path, stream, header, (method == "ascii"));
    if (trimesh.vertices.empty())
        return trimesh;

    // Cleanup
    // TODO: This does not work due to fp precision problems
    // const size_t removedBadAreas = trimesh.removeZeroAreaTriangles();
    // if (removedBadAreas != 0)
    //     IG_LOG(L_WARNING) << "PlyFile " << path << ": Removed " << removedBadAreas << " triangles with zero area" << std::endl;

    // Normals
    bool hasBadAreas = false;
    trimesh.computeFaceNormals(&hasBadAreas);
    // if (hasBadAreas)
    //     IG_LOG(L_WARNING) << "PlyFile " << path << ": Triangle mesh contains triangles with zero area" << std::endl;

    if (trimesh.normals.empty()) {
        IG_LOG(L_WARNING) << "PlyFile " << path << ": No normals are present, computing smooth approximation." << std::endl;
        trimesh.computeVertexNormals();
    } else {
        bool hasBadNormals = false;
        trimesh.fixNormals(&hasBadNormals);
        if (hasBadNormals)
            IG_LOG(L_WARNING) << "PlyFile " << path << ": Some normals were incorrect and thus had to be replaced with arbitrary values." << std::endl;
    }

    if (trimesh.texcoords.empty()) {
        IG_LOG(L_WARNING) << "PlyFile " << path << ": No texture coordinates are present, using default value." << std::endl;
        trimesh.makeTexCoordsZero();
    }

    return trimesh;
}
} // namespace ply
} // namespace IG