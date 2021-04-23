#include "BufferSerializer.h"

namespace IG {
BufferSerializer::BufferSerializer()
	: Serializer(false)
	, mSource(nullptr)
	, mBuffer()
	, mIt(0)
	, mAvailableIt(0)
{
}

BufferSerializer::BufferSerializer(Serializer* source, size_t bufferSize)
	: Serializer(source->isReadMode())
	, mSource(source)
	, mBuffer(bufferSize)
	, mIt(0)
	, mAvailableIt(0)
{
}

BufferSerializer::~BufferSerializer()
{
}

void BufferSerializer::resize(size_t newSize)
{
	mBuffer.resize(newSize);
	mIt			 = 0;
	mAvailableIt = 0;
}

void BufferSerializer::reset(Serializer* source, size_t bufferSize)
{
	mSource = source;
	setReadMode(source->isReadMode());
	resize(bufferSize);
}

bool BufferSerializer::isValid() const
{
	return mSource != nullptr && mSource->isValid();
}

void BufferSerializer::flush()
{
	if (!isReadMode()) {
		if (mIt == 0)
			return;

		mSource->writeRaw(mBuffer.data(), mIt);
		mIt			 = 0;
		mAvailableIt = 0;
	} else if (mIt == mAvailableIt && mAvailableIt == mBuffer.size()) { // End of buffer
		mIt			 = 0;
		mAvailableIt = 0;
	}
}

void BufferSerializer::fetch()
{
	if (!isReadMode() || mIt < mAvailableIt /* Still data available */)
		return;

	flush();

	size_t remaining = mBuffer.size() - mAvailableIt;
	IG_ASSERT(remaining > 0, "After flush there should be some space!");
	mAvailableIt += mSource->readRaw(mBuffer.data() + mAvailableIt, remaining);
}

size_t BufferSerializer::writeRaw(const uint8* data, size_t size)
{
	IG_ASSERT(isValid(), "Trying to write into a close buffer!");
	IG_ASSERT(!isReadMode(), "Trying to write into a read serializer!");

	size_t total = 0;
	while (total < size) {
		size_t remainingBuffer = mBuffer.size() - mIt;
		size_t remainingSrc	   = size - total;
		size_t writesize	   = std::min(remainingBuffer, remainingSrc);

		std::memcpy(mBuffer.data() + mIt, data + total, writesize);
		mIt += writesize;
		total += writesize;

		if (mIt >= mBuffer.size()) {
			flush();
			if (!mSource->isValid())
				return total;
		}
	}

	return size;
}

size_t BufferSerializer::readRaw(uint8* data, size_t size)
{
	IG_ASSERT(isValid(), "Trying to read from a close buffer!");
	IG_ASSERT(isReadMode(), "Trying to read from a write serializer!");
	IG_ASSERT(mIt <= mAvailableIt, "Read iterator has to be lower than buffer iterator");

	size_t total = 0;
	while (total < size) {
		size_t remainingBuffer = mAvailableIt - mIt;
		size_t remainingSrc	   = size - total;
		size_t readsize		   = std::min(remainingBuffer, remainingSrc);

		if (remainingBuffer == 0) {
			fetch();
			if (!mSource->isValid())
				return total;

			remainingBuffer = mAvailableIt - mIt;
			readsize		= std::min(remainingBuffer, remainingSrc);
			IG_ASSERT(remainingBuffer > 0, "Expected atleast some data to be fetched");
		}

		std::memcpy(data + total, mBuffer.data() + mIt, readsize);
		mIt += readsize;
		total += readsize;
	}

	return size;
}

} // namespace IG