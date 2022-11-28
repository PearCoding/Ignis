#include "MtsSerializedFile.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

#include <zlib.h>

namespace IG::mts {
constexpr size_t BUFFER_SIZE = 32768;
class CompressedStream {
    IG_CLASS_NON_COPYABLE(CompressedStream);
    IG_CLASS_NON_MOVEABLE(CompressedStream);

public:
    inline CompressedStream(std::istream& in, size_t size)
        : mIn(in)
        , mSize(size)
        , mPos(0)
        , mStream()
        , mBuffer()
    {
        mStream.zalloc   = Z_NULL;
        mStream.zfree    = Z_NULL;
        mStream.opaque   = Z_NULL;
        mStream.avail_in = 0;
        mStream.next_in  = Z_NULL;

        int retval = inflateInit2(&mStream, 15);
        if (retval != Z_OK)
            IG_LOG(L_ERROR) << "Could not initialize ZLIB: " << retval << std::endl;
    }

    inline ~CompressedStream()
    {
        inflateEnd(&mStream);
    }

    template <typename T>
    inline void read(T* ptr)
    {
        size_t size        = sizeof(T);
        uint8_t* targetPtr = (uint8_t*)ptr;
        while (size > 0) {
            if (mStream.avail_in == 0) {
                size_t remaining = mSize - mPos;
                mStream.next_in  = mBuffer.data();
                mStream.avail_in = (uInt)std::min(remaining, mBuffer.size());
                if (mStream.avail_in == 0)
                    IG_LOG(L_ERROR) << "Read less data than expected (" << FormatMemory(size) << " missing)" << std::endl;
                mIn.read(reinterpret_cast<char*>(mBuffer.data()), mStream.avail_in);

                if (!mIn.good())
                    IG_LOG(L_ERROR) << "Could not read " << FormatMemory(mStream.avail_in) << std::endl;

                mPos += mStream.avail_in;
            }

            mStream.avail_out = (uInt)size;
            mStream.next_out  = targetPtr;

            int retval = inflate(&mStream, Z_NO_FLUSH);
            switch (retval) {
            case Z_STREAM_ERROR:
                IG_LOG(L_ERROR) << "inflate(): stream error!" << std::endl;
                break;
            case Z_NEED_DICT:
                IG_LOG(L_ERROR) << "inflate(): need dictionary!" << std::endl;
                break;
            case Z_DATA_ERROR:
                IG_LOG(L_ERROR) << "inflate(): data error!" << std::endl;
                break;
            case Z_MEM_ERROR:
                IG_LOG(L_ERROR) << "inflate(): memory error!" << std::endl;
                break;
            };

            size_t outputSize = size - (size_t)mStream.avail_out;
            targetPtr += outputSize;
            size -= outputSize;

            if (size > 0 && retval == Z_STREAM_END)
                IG_LOG(L_ERROR) << "inflate(): attempting to read past the end of the stream!" << std::endl;
        }
    }

private:
    std::istream& mIn;
    size_t mSize;
    size_t mPos;
    z_stream mStream;
    std::array<uint8_t, BUFFER_SIZE> mBuffer;
};

enum MeshFlags {
    MF_VERTEXNORMALS = 0x0001,
    MF_TEXCOORDS     = 0x0002,
    MF_VERTEXCOLORS  = 0x0008,
    MF_FACENORMALS   = 0x0010,
    MF_FLOAT         = 0x1000,
    MF_DOUBLE        = 0x2000,
};

template <typename T>
void extractMeshVertices(TriMesh& tri_mesh, CompressedStream& cin, uint32_t flags)
{
    // Vertex Positions
    for (auto& v : tri_mesh.vertices) {
        T x, y, z;
        cin.read(&x);
        cin.read(&y);
        cin.read(&z);
        v = Vector3f((float)x, (float)y, (float)z);
    }

    // Normals
    if (flags & MF_VERTEXNORMALS) {
        for (auto& n : tri_mesh.normals) {
            T x, y, z;
            cin.read(&x);
            cin.read(&y);
            cin.read(&z);
            n = Vector3f((float)x, (float)y, (float)z);
        }
    }

    // UV
    if (flags & MF_TEXCOORDS) {
        for (auto& uv : tri_mesh.texcoords) {
            T x, y;
            cin.read(&x);
            cin.read(&y);
            uv = Vector2f((float)x, (float)y);
        }
    }

    // Vertex Color (ignored)
    if (flags & MF_VERTEXCOLORS) {
        for (size_t i = 0; i < tri_mesh.vertices.size() * 3; ++i) {
            T _ignore;
            cin.read(&_ignore);
        }
    }
}

template <typename T>
void extractMeshIndices(TriMesh& tri_mesh, CompressedStream& cin)
{
    size_t tricount = tri_mesh.indices.size() / 4;
    // Indices
    for (size_t i = 0; i < tricount; ++i) {
        T x, y, z;
        cin.read(&x);
        cin.read(&y);
        cin.read(&z);
        tri_mesh.indices[i * 4 + 0] = (uint32)x;
        tri_mesh.indices[i * 4 + 1] = (uint32)y;
        tri_mesh.indices[i * 4 + 2] = (uint32)z;
        tri_mesh.indices[i * 4 + 3] = 0;
    }
}

TriMesh load(const std::filesystem::path& path, size_t shapeIndex)
{
    std::fstream stream(path, std::ios::in | std::ios::binary);
    if (!stream) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' can not be opened." << std::endl;
        return TriMesh{};
    }

    // Check header
    uint16_t fileIdent   = 0;
    uint16_t fileVersion = 0;
    stream.read(reinterpret_cast<char*>(&fileIdent), sizeof(fileIdent));

    if (fileIdent != 0x041C) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' is not a valid Mitsuba serialized file." << std::endl;
        return TriMesh{};
    }
    stream.read(reinterpret_cast<char*>(&fileVersion), sizeof(fileVersion));
    if (fileVersion < 3) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' has an insufficient version number " << fileVersion << " < 3." << std::endl;
        return TriMesh{};
    }

    // Extract amount of shapes inside the file
    uint32_t shapeCount = 0;
    stream.seekg(-std::streamoff(sizeof(shapeCount)), std::ios::end);
    stream.read(reinterpret_cast<char*>(&shapeCount), sizeof(shapeCount));

    if (!stream.good()) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' can not access end of file dictionary." << std::endl;
        return TriMesh{};
    }

    if (shapeIndex >= shapeCount) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' can not access shape index " << shapeIndex << " as it only contains " << shapeCount << " shapes." << std::endl;
        return TriMesh{};
    }

    // Extract mesh file start position
    uint64_t shapeFileStart = 0;
    uint64_t shapeFileEnd   = 0;

    if (fileVersion >= 4) {
        stream.seekg(-std::streamoff(sizeof(shapeCount) + sizeof(shapeFileStart) * (shapeCount - shapeIndex)), std::ios::end);
        stream.read(reinterpret_cast<char*>(&shapeFileStart), sizeof(shapeFileStart));

        if (!stream.good()) {
            IG_LOG(L_ERROR) << "Given file '" << path << "' could not extract shape file offset." << std::endl;
            return TriMesh{};
        }

        // Extract mesh file end position
        if (shapeIndex == shapeCount - 1) {
            stream.seekg(-std::streamoff(sizeof(shapeCount)), std::ios::end);
            shapeFileEnd = stream.tellg();
        } else {
            stream.seekg(-std::streamoff(sizeof(shapeCount) + sizeof(shapeFileEnd) * (shapeCount - shapeIndex + 1)), std::ios::end);
            stream.read(reinterpret_cast<char*>(&shapeFileEnd), sizeof(shapeFileEnd));
        }
    } else { /* Version 3 uses uint32_t instead of uint64_t */
        uint32_t _shapeFileStart = 0;
        uint32_t _shapeFileEnd   = 0;

        stream.seekg(-std::streamoff(sizeof(shapeCount) + sizeof(_shapeFileStart) * (shapeCount - shapeIndex)), std::ios::end);
        stream.read(reinterpret_cast<char*>(&_shapeFileStart), sizeof(_shapeFileStart));

        if (!stream.good()) {
            IG_LOG(L_ERROR) << "Given file '" << path << "' could not extract shape file offset." << std::endl;
            return TriMesh{};
        }

        // Extract mesh file end position
        if (shapeIndex == shapeCount - 1) {
            stream.seekg(-std::streamoff(sizeof(shapeCount)), std::ios::end);
            _shapeFileEnd = (uint32_t)stream.tellg();
        } else {
            stream.seekg(-std::streamoff(sizeof(shapeCount) + sizeof(_shapeFileEnd) * (shapeCount - shapeIndex + 1)), std::ios::end);
            stream.read(reinterpret_cast<char*>(&_shapeFileEnd), sizeof(_shapeFileEnd));
        }

        shapeFileStart = _shapeFileStart;
        shapeFileEnd   = _shapeFileEnd;
    }

    if (!stream.good()) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' could not extract shape file offset." << std::endl;
        return TriMesh{};
    }

    const size_t maxContentSize = shapeFileEnd - shapeFileStart - sizeof(uint16_t) * 2;

    // Go to start position of mesh
    stream.seekg(sizeof(uint16_t) * 2 /*Header*/ + shapeFileStart, std::ios::beg);

    // Inflate with zlib
    CompressedStream cin(stream, maxContentSize);

    uint32_t mesh_flags = 0;
    cin.read(&mesh_flags);

    if (fileVersion >= 4) {
        uint8_t utf8Char = 0;
        do {
            cin.read(&utf8Char);
        } while (utf8Char != 0); // Ignore shape name
    }

    uint64_t vertexCount = 0;
    uint64_t triCount    = 0;
    cin.read(&vertexCount);
    cin.read(&triCount);

    if (vertexCount == 0 || triCount == 0) {
        IG_LOG(L_ERROR) << "Given file '" << path << "' has no valid mesh." << std::endl;
        return TriMesh{};
    }

    TriMesh tri_mesh;
    tri_mesh.vertices.resize(vertexCount);
    tri_mesh.normals.resize(vertexCount);
    tri_mesh.texcoords.resize(vertexCount);
    tri_mesh.indices.resize(triCount * 4);

    if (mesh_flags & MF_DOUBLE)
        extractMeshVertices<double>(tri_mesh, cin, mesh_flags);
    else
        extractMeshVertices<float>(tri_mesh, cin, mesh_flags);

    if (vertexCount > 0xFFFFFFFF)
        extractMeshIndices<uint64_t>(tri_mesh, cin);
    else
        extractMeshIndices<uint32_t>(tri_mesh, cin);

    // Cleanup
    // TODO: This does not work due to fp precision problems
    // const size_t removedBadAreas = tri_mesh.removeZeroAreaTriangles();
    // if (removedBadAreas != 0)
    //     IG_LOG(L_WARNING) << "MtsFile " << path << ": Removed " << removedBadAreas << " triangles with zero area" << std::endl;

    // Normals
    if (!(mesh_flags & MF_VERTEXNORMALS)) {
        IG_LOG(L_INFO) << "MtsFile " << path << ": No normals are present, computing smooth approximation." << std::endl;
        tri_mesh.computeVertexNormals();
    } else {
        bool hasBadNormals = false;
        tri_mesh.fixNormals(&hasBadNormals);
        if (hasBadNormals)
            IG_LOG(L_WARNING) << "MtsFile " << path << ": Some normals were incorrect and thus had to be replaced with arbitrary values." << std::endl;
    }

    if (!(mesh_flags & MF_TEXCOORDS)) {
        IG_LOG(L_INFO) << "MtsFile " << path << ": No texture coordinates are present, using default value." << std::endl;
        tri_mesh.makeTexCoordsNormalized();
    }

    return tri_mesh;
}
} // namespace IG::mts