#pragma once

#include "Buffer.h"
#include "bvh/BVH.h"
#include "math/Triangle.h"
#include "mesh/TriMesh.h"

#include "Target.h"

// Contains implementation for NodeN and TriN
#include "generated_interface.h"

namespace IG {
template <size_t N, size_t M>
struct BvhNTriM {
};

template <>
struct BvhNTriM<8, 4> {
	using Node = Node8;
	using Tri  = Tri4;
};

template <>
struct BvhNTriM<4, 4> {
	using Node = Node4;
	using Tri  = Tri4;
};

template <>
struct BvhNTriM<2, 1> {
	using Node = Node2;
	using Tri  = Tri1;
};

template <size_t N, size_t M>
class BvhNTriMAdapter {
	struct CostFn {
		static float leaf_cost(int count, float area)
		{
			return count * area;
		}
		static float traversal_cost(float area)
		{
			return area;
		}
	};

	using BvhBuilder = SplitBvhBuilder<N, CostFn>;
	using Adapter	 = BvhNTriMAdapter;
	using Node		 = typename BvhNTriM<N, M>::Node;
	using Tri		 = typename BvhNTriM<N, M>::Tri;

	std::vector<Node>& nodes_;
	std::vector<Tri>& tris_;
	BvhBuilder builder_;

public:
	BvhNTriMAdapter(std::vector<Node>& nodes, std::vector<Tri>& tris)
		: nodes_(nodes)
		, tris_(tris)
	{
	}

	void build(const TriMesh& tri_mesh, const std::vector<IG::Triangle>& tris)
	{
		builder_.build(tris, NodeWriter(*this), LeafWriter(*this, tris, tri_mesh.indices), M / 2);
	}

private:
	struct NodeWriter {
		Adapter& adapter;

		NodeWriter(Adapter& adapter)
			: adapter(adapter)
		{
		}

		template <typename BBoxFn>
		int operator()(int parent, int child, const BoundingBox& /*parent_bb*/, size_t count, BBoxFn bboxes)
		{
			auto& nodes = adapter.nodes_;

			size_t i = nodes.size();
			nodes.emplace_back();

			if (parent >= 0 && child >= 0) {
				assert(parent >= 0 && parent < nodes.size());
				assert(child >= 0 && child < N);
				nodes[parent].child[child] = i + 1;
			}

			assert(count >= 2 && count <= N);

			for (size_t j = 0; j < count; j++) {
				const BoundingBox& bbox = bboxes(j);
				nodes[i].bounds[0][j]	= bbox.min.x;
				nodes[i].bounds[2][j]	= bbox.min.y;
				nodes[i].bounds[4][j]	= bbox.min.z;

				nodes[i].bounds[1][j] = bbox.max.x;
				nodes[i].bounds[3][j] = bbox.max.y;
				nodes[i].bounds[5][j] = bbox.max.z;
			}

			for (size_t j = count; j < N; ++j) {
				nodes[i].bounds[0][j] = Inf;
				nodes[i].bounds[2][j] = Inf;
				nodes[i].bounds[4][j] = Inf;

				nodes[i].bounds[1][j] = -Inf;
				nodes[i].bounds[3][j] = -Inf;
				nodes[i].bounds[5][j] = -Inf;

				nodes[i].child[j] = 0;
			}

			return i;
		}
	};

	struct LeafWriter {
		Adapter& adapter;
		const std::vector<IG::Triangle>& in_tris;
		const std::vector<uint32_t>& indices;

		LeafWriter(Adapter& adapter, const std::vector<IG::Triangle>& in_tris, const std::vector<uint32_t>& indices)
			: adapter(adapter)
			, in_tris(in_tris)
			, indices(indices)
		{
		}

		template <typename RefFn>
		void operator()(int parent, int child, const BoundingBox& /*leaf_bb*/, size_t ref_count, RefFn refs)
		{
			auto& nodes = adapter.nodes_;
			auto& tris	= adapter.tris_;

			nodes[parent].child[child] = ~tris.size();

			// Group triangles by packets of M
			for (size_t i = 0; i < ref_count; i += M) {
				const size_t c = i + M <= ref_count ? M : ref_count - i;

				Tri tri;
				std::memset(&tri, 0, sizeof(Tri));
				for (size_t j = 0; j < c; j++) {
					const int id	  = refs(i + j);
					auto& in_tri	  = in_tris[id];
					const Vector3f e1 = in_tri.v0 - in_tri.v1;
					const Vector3f e2 = in_tri.v2 - in_tri.v0;
					const Vector3f n  = e1.cross(e2);
					tri.v0[0][j]	  = in_tri.v0.x;
					tri.v0[1][j]	  = in_tri.v0.y;
					tri.v0[2][j]	  = in_tri.v0.z;

					tri.e1[0][j] = e1.x;
					tri.e1[1][j] = e1.y;
					tri.e1[2][j] = e1.z;

					tri.e2[0][j] = e2.x;
					tri.e2[1][j] = e2.y;
					tri.e2[2][j] = e2.z;

					tri.n[0][j] = n.x;
					tri.n[1][j] = n.y;
					tri.n[2][j] = n.z;

					tri.prim_id[j] = id;
					tri.geom_id[j] = indices[id * 4 + 3];
				}

				for (size_t j = c; j < 4; j++)
					tri.prim_id[j] = 0xFFFFFFFF;

				tris.emplace_back(tri);
			}
			assert(ref_count > 0);
			tris.back().prim_id[M - 1] |= 0x80000000;
		}
	};
};

template <>
class BvhNTriMAdapter<2, 1> {
	struct CostFn {
		static float leaf_cost(int count, float area)
		{
			return count * area;
		}
		static float traversal_cost(float area)
		{
			return area;
		}
	};

	using BvhBuilder = SplitBvhBuilder<2, CostFn>;
	using Adapter	 = BvhNTriMAdapter;
	using Node		 = Node2;
	using Tri		 = Tri1;

	std::vector<Node>& nodes_;
	std::vector<Tri>& tris_;
	BvhBuilder builder_;

public:
	BvhNTriMAdapter(std::vector<Node>& nodes, std::vector<Tri>& tris)
		: nodes_(nodes)
		, tris_(tris)
	{
	}

	void build(const TriMesh& tri_mesh, const std::vector<IG::Triangle>& tris)
	{
		builder_.build(tris, NodeWriter(*this), LeafWriter(*this, tris, tri_mesh.indices), 2);
	}

private:
	struct NodeWriter {
		Adapter& adapter;

		NodeWriter(Adapter& adapter)
			: adapter(adapter)
		{
		}

		template <typename BBoxFn>
		int operator()(int parent, int child, const BoundingBox& /*parent_bb*/, size_t count, BBoxFn bboxes)
		{
			auto& nodes = adapter.nodes_;

			size_t i = nodes.size();
			nodes.emplace_back();

			if (parent >= 0 && child >= 0) {
				assert(parent >= 0 && parent < nodes.size());
				assert(child >= 0 && child < 2);
				nodes[parent].child.e[child] = i + 1;
			}

			assert(count >= 1 && count <= 2);

			const BoundingBox& bbox1 = bboxes(0);
			nodes[i].bounds.e[0]	 = bbox1.min(0);
			nodes[i].bounds.e[2]	 = bbox1.min(1);
			nodes[i].bounds.e[4]	 = bbox1.min(2);
			nodes[i].bounds.e[1]	 = bbox1.max(0);
			nodes[i].bounds.e[3]	 = bbox1.max(1);
			nodes[i].bounds.e[5]	 = bbox1.max(2);

			if (count == 2) {
				const BoundingBox& bbox2 = bboxes(1);
				nodes[i].bounds.e[6]	 = bbox2.min(0);
				nodes[i].bounds.e[8]	 = bbox2.min(1);
				nodes[i].bounds.e[10]	 = bbox2.min(2);
				nodes[i].bounds.e[7]	 = bbox2.max(0);
				nodes[i].bounds.e[9]	 = bbox2.max(1);
				nodes[i].bounds.e[11]	 = bbox2.max(2);
			} else {
				nodes[i].bounds.e[6]  = Inf;
				nodes[i].bounds.e[8]  = Inf;
				nodes[i].bounds.e[10] = Inf;
				nodes[i].bounds.e[7]  = -Inf;
				nodes[i].bounds.e[9]  = -Inf;
				nodes[i].bounds.e[11] = -Inf;
			}

			return i;
		}
	};

	struct LeafWriter {
		Adapter& adapter;
		const std::vector<IG::Triangle>& in_tris;
		const std::vector<uint32_t>& indices;

		LeafWriter(Adapter& adapter, const std::vector<IG::Triangle>& in_tris, const std::vector<uint32_t>& indices)
			: adapter(adapter)
			, in_tris(in_tris)
			, indices(indices)
		{
		}

		template <typename RefFn>
		void operator()(int parent, int child, const BoundingBox& /*leaf_bb*/, size_t ref_count, RefFn refs)
		{
			auto& nodes = adapter.nodes_;
			auto& tris	= adapter.tris_;

			nodes[parent].child.e[child] = ~tris.size();

			for (int i = 0; i < ref_count; i++) {
				const int ref	  = refs(i);
				auto& tri		  = in_tris[ref];
				const Vector3f e1 = tri.v0 - tri.v1;
				const Vector3f e2 = tri.v2 - tri.v0;
				const Vector3f n  = e1.cross(e2);
				int geom_id		  = indices[ref * 4 + 3];
				tris.emplace_back(Tri1{
					{ tri.v0(0), tri.v0(1), tri.v0(2) }, 0, { e1(0), e1(1), e1(2) }, geom_id, { e2(0), e2(1), e2(2) }, ref });
			}

			// Add sentinel
			tris.back().prim_id |= 0x80000000;
		}
	};
};

template <size_t N, size_t M>
inline void build_bvh(const TriMesh& tri_mesh,
					  std::vector<typename BvhNTriM<N, M>::Node>& nodes,
					  std::vector<typename BvhNTriM<N, M>::Tri>& tris)
{
	BvhNTriMAdapter<N, M> adapter(nodes, tris);
	auto num_tris = tri_mesh.indices.size() / 4;
	std::vector<IG::Triangle> in_tris(num_tris);
	for (size_t i = 0; i < num_tris; i++) {
		auto& v0   = tri_mesh.vertices[tri_mesh.indices[i * 4 + 0]];
		auto& v1   = tri_mesh.vertices[tri_mesh.indices[i * 4 + 1]];
		auto& v2   = tri_mesh.vertices[tri_mesh.indices[i * 4 + 2]];
		in_tris[i] = Triangle(v0, v1, v2);
	}
	adapter.build(tri_mesh, in_tris);
}

template <typename Node, typename Tri>
inline void write_bvh(std::vector<Node>& nodes, std::vector<Tri>& tris)
{
	std::ofstream of("data/bvh.bin", std::ios::app | std::ios::binary);
	size_t node_size = sizeof(Node);
	size_t tri_size	 = sizeof(Tri);
	of.write((char*)&node_size, sizeof(uint32_t));
	of.write((char*)&tri_size, sizeof(uint32_t));
	write_buffer(of, nodes);
	write_buffer(of, tris);
}

inline bool must_build_bvh(const std::string& name, Target target)
{
	std::ifstream bvh_stamp("data/bvh.stamp", std::fstream::in);
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
} // namespace IG