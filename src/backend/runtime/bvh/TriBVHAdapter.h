#pragma once

#include "BVH.h"
#include "math/Triangle.h"
#include "mesh/TriMesh.h"

#include "Target.h"

// Contains implementation for NodeN and TriN
#include "generated_interface.h"

namespace IG {

template <>
struct ObjectAdapter<Triangle> {
	const Triangle& tri;

	ObjectAdapter(const Triangle& t)
		: tri(t)
	{
	}

	inline BoundingBox computeBoundingBox() const
	{
		return tri.computeBoundingBox();
	}

	inline Vector3f center() const
	{
		return (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
	}

	inline void computeSplit(IG::BoundingBox& left_bb, IG::BoundingBox& right_bb, int axis, float split) const
	{
		tri.computeSplit(left_bb, right_bb, axis, split);
	}
};

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

	using BvhBuilder = SplitBvhBuilder<Triangle, N, CostFn>;
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
#ifdef STATISTICS
		builder_.print_stats();
#endif
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
				nodes[parent].child.e[child] = i + 1;
			}

			assert(count >= 2 && count <= N);

			for (size_t j = 0; j < count; j++) {
				const BoundingBox& bbox	  = bboxes(j);
				nodes[i].bounds.e[0].e[j] = bbox.min(0);
				nodes[i].bounds.e[2].e[j] = bbox.min(1);
				nodes[i].bounds.e[4].e[j] = bbox.min(2);

				nodes[i].bounds.e[1].e[j] = bbox.max(0);
				nodes[i].bounds.e[3].e[j] = bbox.max(1);
				nodes[i].bounds.e[5].e[j] = bbox.max(2);
			}

			for (size_t j = count; j < N; ++j) {
				nodes[i].bounds.e[0].e[j] = FltInf;
				nodes[i].bounds.e[2].e[j] = FltInf;
				nodes[i].bounds.e[4].e[j] = FltInf;

				nodes[i].bounds.e[1].e[j] = -FltInf;
				nodes[i].bounds.e[3].e[j] = -FltInf;
				nodes[i].bounds.e[5].e[j] = -FltInf;

				nodes[i].child.e[j] = 0;
			}

			return i;
		}
	};

	struct LeafWriter {
		Adapter& adapter;
		const std::vector<IG::Triangle>& in_tris;
		const std::vector<uint32>& indices;

		LeafWriter(Adapter& adapter, const std::vector<IG::Triangle>& in_tris, const std::vector<uint32>& indices)
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
					tri.v0.e[0].e[j]  = in_tri.v0(0);
					tri.v0.e[1].e[j]  = in_tri.v0(1);
					tri.v0.e[2].e[j]  = in_tri.v0(2);

					tri.e1.e[0].e[j] = e1(0);
					tri.e1.e[1].e[j] = e1(1);
					tri.e1.e[2].e[j] = e1(2);

					tri.e2.e[0].e[j] = e2(0);
					tri.e2.e[1].e[j] = e2(1);
					tri.e2.e[2].e[j] = e2(2);

					tri.n.e[0].e[j] = n(0);
					tri.n.e[1].e[j] = n(1);
					tri.n.e[2].e[j] = n(2);

					tri.prim_id.e[j] = id;
				}

				for (size_t j = c; j < M; j++)
					tri.prim_id.e[j] = 0xFFFFFFFF;

				tris.emplace_back(tri);
			}
			assert(ref_count > 0);
			tris.back().prim_id.e[M - 1] |= 0x80000000;
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

	using BvhBuilder = SplitBvhBuilder<Triangle, 2, CostFn>;
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
#ifdef STATISTICS
		builder_.print_stats();
#endif
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
				nodes[i].bounds.e[6]  = FltInf;
				nodes[i].bounds.e[8]  = FltInf;
				nodes[i].bounds.e[10] = FltInf;
				nodes[i].bounds.e[7]  = -FltInf;
				nodes[i].bounds.e[9]  = -FltInf;
				nodes[i].bounds.e[11] = -FltInf;
			}

			return i;
		}
	};

	struct LeafWriter {
		Adapter& adapter;
		const std::vector<IG::Triangle>& in_tris;
		const std::vector<uint32>& indices;

		LeafWriter(Adapter& adapter, const std::vector<IG::Triangle>& in_tris, const std::vector<uint32>& indices)
			: adapter(adapter)
			, in_tris(in_tris)
			, indices(indices)
		{
		}

		template <typename RefFn>
		void operator()(int parent, int child, const BoundingBox& /*leaf_bb*/, size_t ref_count, RefFn refs)
		{
			assert(ref_count > 0);

			auto& nodes = adapter.nodes_;
			auto& tris	= adapter.tris_;

			nodes[parent].child.e[child] = ~tris.size();

			for (size_t i = 0; i < ref_count; i++) {
				const int ref	  = refs(i);
				const auto& tri	  = in_tris[ref];
				const Vector3f e1 = tri.v0 - tri.v1;
				const Vector3f e2 = tri.v2 - tri.v0;
				tris.emplace_back(Tri1{
					{ tri.v0(0), tri.v0(1), tri.v0(2) }, 0, { e1(0), e1(1), e1(2) }, 0, { e2(0), e2(1), e2(2) }, ref });
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
} // namespace IG