#pragma once

#include "IG_Config.h"

namespace IG {
struct TriMesh;

namespace IO {
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

void write_tri_mesh(const std::string& suffix, const TriMesh& tri_mesh, bool enable_padding);
} // namespace IO
} // namespace IG