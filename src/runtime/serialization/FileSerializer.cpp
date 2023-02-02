#include "FileSerializer.h"

#include <fstream>

namespace IG {
struct FileSerializerInternal {
    std::fstream File;
    size_t MemoryFootprint = 0;
};

FileSerializer::FileSerializer()
    : Serializer(false)
    , mInternal(std::make_unique<FileSerializerInternal>())
{
}

FileSerializer::FileSerializer(const Path& path, bool readmode)
    : Serializer(readmode)
    , mInternal(std::make_unique<FileSerializerInternal>())
{
    open(path, readmode);
}

FileSerializer::~FileSerializer()
{
    close();
}

size_t FileSerializer::memoryFootprint() const
{
    return mInternal->MemoryFootprint;
}

bool FileSerializer::open(const Path& path, bool readmode)
{
    if (mInternal->File.is_open())
        return false;

    auto flags = (readmode ? std::ios_base::in : std::ios_base::out) | std::ios_base::binary;
    mInternal->File.open(path.c_str(), flags);
    if (!mInternal->File)
        return false;

    setReadMode(readmode);

    return true;
}

void FileSerializer::close()
{
    if (mInternal->File.is_open())
        mInternal->File.close();
}

bool FileSerializer::isValid() const
{
    return mInternal && mInternal->File.is_open();
}

size_t FileSerializer::currentSize() const
{
    return mInternal->MemoryFootprint;
}

size_t FileSerializer::writeRaw(const uint8* data, size_t size)
{
    IG_ASSERT(isValid(), "Trying to write into a close buffer!");
    IG_ASSERT(!isReadMode(), "Trying to write into a read serializer!");

    mInternal->MemoryFootprint += size;
    mInternal->File.write(reinterpret_cast<const char*>(data), size);
    return size;
}

size_t FileSerializer::readRaw(uint8* data, size_t size)
{
    IG_ASSERT(isValid(), "Trying to read from a close buffer!");
    IG_ASSERT(isReadMode(), "Trying to read from a write serializer!");

    mInternal->MemoryFootprint += size;
    mInternal->File.read(reinterpret_cast<char*>(data), size);
    return size; // TODO: Really??
}

} // namespace IG