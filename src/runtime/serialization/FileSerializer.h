#pragma once

#include "Serializer.h"

#include <filesystem>

namespace IG {
class FileSerializer : public Serializer {
public:
    FileSerializer();
    FileSerializer(const Path& path, bool readmode);
    virtual ~FileSerializer();

    bool open(const Path& path, bool readmode);
    void close();

    size_t memoryFootprint() const;

    // Interface
    virtual bool isValid() const override;
    virtual size_t writeRaw(const uint8* data, size_t size) override;
    virtual size_t readRaw(uint8* data, size_t size) override;
    virtual size_t currentSize() const override;

private:
    std::unique_ptr<struct FileSerializerInternal> mInternal;
};
} // namespace IG