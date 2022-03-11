#pragma once

#include "Serializer.h"

namespace IG {
class BufferSerializer : public Serializer {
public:
    BufferSerializer();
    explicit BufferSerializer(Serializer* source, size_t bufferSize = 1024);
    virtual ~BufferSerializer();

    void resize(size_t newSize);
    inline size_t maxSize() const { return mBuffer.size(); }
    inline size_t currentUsed() const { return mIt; }

    void flush();

    // Interface
    virtual bool isValid() const override;
    virtual size_t writeRaw(const uint8* data, size_t size) override;
    virtual size_t readRaw(uint8* data, size_t size) override;

protected:
    void reset(Serializer* source, size_t bufferSize);

private:
    void fetch();

    Serializer* mSource;
    std::vector<uint8> mBuffer;
    size_t mIt;
    size_t mAvailableIt; // Iterator showing available data, which is the whole buffer in case of write-mode all the time. This is only useful in read-mode
};
} // namespace IG