#pragma once

#include <fstream>
#include <istream>
#include <ostream>

#include "IG_Config.h"

// #define IG_NO_COMPRESS

#ifndef IG_NO_COMPRESS
#include <lz4.h>
#endif

namespace IG {
namespace IO {
inline void skip_buffer(std::istream& is)
{
	size_t in_size = 0, out_size = 0;
	is.read((char*)&in_size, sizeof(uint32));
	is.read((char*)&out_size, sizeof(uint32));
	is.seekg(out_size, std::ios::cur);
}

template <typename Array>
inline void decompress(const std::vector<char>& in, Array& out)
{
#ifndef IG_NO_COMPRESS
	LZ4_decompress_safe(in.data(), (char*)out.data(), in.size(), out.size() * sizeof(out[0]));
#else
	std::memcpy((char*)out.data(), in.data(), out.size() * sizeof(out[0]));
#endif
}

template <typename Array>
inline void read_buffer(std::istream& is, Array& array)
{
	size_t in_size = 0, out_size = 0;
	is.read((char*)&in_size, sizeof(uint32));
	is.read((char*)&out_size, sizeof(uint32));
	std::vector<char> in(out_size);
	is.read(in.data(), in.size());
	array = std::move(Array(in_size / sizeof(array[0])));
	decompress(in, array);
}

template <typename Array>
inline void read_buffer(const std::string& file_name, Array& array)
{
	std::ifstream is(file_name, std::ios::binary);
	read_buffer(is, array);
}

template <typename Array>
inline void compress(const Array& in, std::vector<char>& out)
{
	const size_t in_size = sizeof(in[0]) * in.size();
#ifndef IG_NO_COMPRESS
	out.resize(LZ4_compressBound(in_size));
	out.resize(LZ4_compress_default((const char*)in.data(), out.data(), in_size, out.size()));
#else
	out.resize(in_size);
	std::memcpy((char*)out.data(), in.data(), in_size);
#endif
}

template <typename Array>
inline void write_buffer(std::ostream& os, const Array& array)
{
	std::vector<char> out;
	compress(array, out);
	size_t in_size	= sizeof(array[0]) * array.size();
	size_t out_size = out.size();
	os.write((char*)&in_size, sizeof(uint32));
	os.write((char*)&out_size, sizeof(uint32));
	os.write(out.data(), out.size());
}

template <typename Array>
inline void write_buffer(const std::string& file_name, const Array& array)
{
	std::ofstream of(file_name, std::ios::binary);
	write_buffer(of, array);
}

template <typename T>
inline std::vector<uint8> pad_buffer(const std::vector<T>& elems, bool enable, size_t size)
{
	std::vector<uint8> new_elems;
	if (!enable) {
		new_elems.resize(sizeof(T) * elems.size());
		memcpy(new_elems.data(), elems.data(), sizeof(T) * elems.size());
		return new_elems;
	}
	assert(size >= sizeof(T));
	new_elems.resize(size * elems.size(), 0);
	uint8* ptr = new_elems.data();
	for (auto& elem : elems) {
		memcpy(ptr, &elem, sizeof(T));
		ptr += size;
	}
	return new_elems;
}
} // namespace IO
} // namespace IG