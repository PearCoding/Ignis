#pragma once

#include "Serializer.h"

namespace IG {
class MemorySerializer : public Serializer {
public:
	MemorySerializer();
	MemorySerializer(uint8* buffer, size_t size, bool readmode);
	virtual ~MemorySerializer();

	inline size_t maxSize() const { return mSize; }

	bool open(uint8* buffer, size_t size, bool readmode);
	void close();

	// Interface
	virtual bool isValid() const override;
	virtual size_t writeRaw(const uint8* data, size_t size) override;
	virtual size_t readRaw(uint8* data, size_t size) override;

private:
	uint8* mBuffer;
	size_t mSize;
	size_t mIt;
};
} // namespace IG