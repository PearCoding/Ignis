#pragma once

#include "Serializer.h"

namespace IG {
class VectorSerializer : public Serializer {
public:
    VectorSerializer(std::vector<uint8>& data, bool readmode);
    virtual ~VectorSerializer() = default;

    void ensureAlignment(size_t alignment);

    // Interface
    virtual bool isValid() const override;
    virtual size_t writeRaw(const uint8* data, size_t size) override;
    virtual size_t readRaw(uint8* data, size_t size) override;

private:
    std::vector<uint8>& mData;
    size_t mIt;
};
} // namespace IG