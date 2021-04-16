#include "MemorySerializer.h"

namespace IG {
MemorySerializer::MemorySerializer()
	: Serializer(false)
	, mBuffer(nullptr)
	, mSize(0)
	, mIt(0)
{
}

MemorySerializer::MemorySerializer(uint8* buffer, size_t size, bool readmode)
	: Serializer(readmode)
	, mBuffer(buffer)
	, mSize(size)
	, mIt(0)
{
	open(buffer, size, readmode);
}

MemorySerializer::~MemorySerializer()
{
	close();
}

bool MemorySerializer::open(uint8* buffer, size_t size, bool readmode)
{
	mBuffer = buffer;
	mSize	= size;
	mIt		= 0;
	if (!mBuffer)
		return false;

	setReadMode(readmode);

	return true;
}

void MemorySerializer::close()
{
	mBuffer = nullptr;
}

bool MemorySerializer::isValid() const
{
	return mBuffer;
}

size_t MemorySerializer::writeRaw(const uint8* data, size_t size)
{
	IG_ASSERT(isValid(), "Trying to write into a close buffer!");
	IG_ASSERT(!isReadMode(), "Trying to write into a read serializer!");

	if (mIt + size > mSize)
		return 0;

	std::memcpy(&mBuffer[mIt], data, size);

	mIt += size;
	return size;
}

size_t MemorySerializer::readRaw(uint8* data, size_t size)
{
	IG_ASSERT(isValid(), "Trying to read from a close buffer!");
	IG_ASSERT(isReadMode(), "Trying to read from a write serializer!");

	const size_t end = std::min(mSize, mIt + size);
	const size_t rem = end - mIt;

	std::memcpy(&data[0], &mBuffer[mIt], rem);
	mIt += rem;

	return rem;
}

} // namespace IG