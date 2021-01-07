#pragma once

#include "Buffer.h"

#include "Target.h"

namespace IG {
namespace IO {
inline std::string bvh_filepath(const std::string& suffix)
{
	if (suffix.empty())
		return "data/bvh.bin";
	else
		return "data/bvh_" + suffix + ".bin";
}

inline std::string bvh_stamp_filepath(const std::string& suffix)
{
	if (suffix.empty())
		return "data/bvh.stamp";
	else
		return "data/bvh_" + suffix + ".stamp";
}

inline void remove_bvh(const std::string& suffix)
{
	const std::string filename = bvh_filepath(suffix);
	std::remove(filename.c_str());
}

template <typename Node, typename Object>
inline void write_bvh(const std::string& suffix, std::vector<Node>& nodes, std::vector<Object>& objs)
{
	std::ofstream of(bvh_filepath(suffix), std::ios::app | std::ios::binary);
	size_t node_size = sizeof(Node);
	size_t obj_size	 = sizeof(Object);
	of.write((char*)&node_size, sizeof(uint32));
	of.write((char*)&obj_size, sizeof(uint32));
	write_buffer(of, nodes);
	write_buffer(of, objs);
}

inline bool must_build_bvh(const std::string& suffix, const std::string& name, Target target)
{
	std::ifstream bvh_stamp(bvh_stamp_filepath(suffix), std::fstream::in);
	if (bvh_stamp) {
		int bvh_target;
		bvh_stamp >> bvh_target;
		if (bvh_target != (int)target)
			return true;
		std::string bvh_name;
		bvh_stamp >> bvh_name;
		if (bvh_name != name)
			return true;
		return false;
	}
	return true;
}

inline void write_bvh_stamp(const std::string& suffix, const std::string& name, Target target)
{
	std::ofstream bvh_stamp(bvh_stamp_filepath(suffix));
	bvh_stamp << int(target) << " " << name;
}
} // namespace IO
} // namespace IG