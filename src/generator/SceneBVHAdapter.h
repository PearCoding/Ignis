#pragma once

#include "bvh/BVH.h"
#include "math/BoundingBox.h"

#include "Target.h"

// Contains implementation for NodeN
#include "generated_interface.h"

namespace IG {

template <>
struct ObjectAdapter<BoundingBox> {
	const BoundingBox& bbox;

	ObjectAdapter(const BoundingBox& t)
		: bbox(t)
	{
	}

	inline BoundingBox computeBoundingBox() const
	{
		return bbox;
	}

	inline Vector3f center() const
	{
		return (bbox.max + bbox.min) * (1.0f / 2.0f);
	}

	inline void computeSplit(IG::BoundingBox& left_bb, IG::BoundingBox& right_bb, int axis, float split) const
	{
		// Should not be used
		IG_ASSERT(false, "Compute Split should not be used when UseSpatialSplits is disabled");
		bbox.computeSplit(left_bb, right_bb, axis, split);
	}
};

template <size_t N>
struct BvhNBbox {
};

template <>
struct BvhNBbox<8> {
	using Node = Node8;
};

template <>
struct BvhNBbox<4> {
	using Node = Node4;
};

template <>
struct BvhNBbox<2> {
	using Node = Node2;
};

template <size_t N>
class BvhNBboxAdapter {
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

	using BvhBuilder = BasicBvhBuilder<BoundingBox, N, CostFn>;
	using Adapter	 = BvhNBboxAdapter;
	using Node		 = typename BvhNBbox<N>::Node;
	using Obj		 = TaggedBBox;

	std::vector<Node>& nodes_;
	std::vector<Obj>& objs_;
	BvhBuilder builder_;

public:
	BvhNBboxAdapter(std::vector<Node>& nodes, std::vector<Obj>& objs)
		: nodes_(nodes)
		, objs_(objs)
	{
	}

	void build(const std::vector<BoundingBox>& objs)
	{
		builder_.build(objs, NodeWriter(*this), LeafWriter(*this, objs), 1);
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
				nodes[i].bounds.e[0].e[j] = Inf;
				nodes[i].bounds.e[2].e[j] = Inf;
				nodes[i].bounds.e[4].e[j] = Inf;

				nodes[i].bounds.e[1].e[j] = -Inf;
				nodes[i].bounds.e[3].e[j] = -Inf;
				nodes[i].bounds.e[5].e[j] = -Inf;

				nodes[i].child.e[j] = 0;
			}

			return i;
		}
	};

	struct LeafWriter {
		Adapter& adapter;
		const std::vector<IG::BoundingBox>& in_objs;

		LeafWriter(Adapter& adapter, const std::vector<IG::BoundingBox>& in_objs)
			: adapter(adapter)
			, in_objs(in_objs)
		{
		}

		template <typename RefFn>
		void operator()(int parent, int child, const BoundingBox& /*leaf_bb*/, size_t ref_count, RefFn refs)
		{
			auto& nodes = adapter.nodes_;
			auto& objs	= adapter.objs_;

			nodes[parent].child.e[child] = ~objs.size();

			for (size_t i = 0; i < ref_count; ++i) {
				const int id	   = refs(i);
				const auto& in_obj = in_objs[id];

				TaggedBBox obj;
				obj.min.x = in_obj.min(0);
				obj.min.y = in_obj.min(1);
				obj.min.z = in_obj.min(2);

				obj.pad0 = 0;
				
				obj.max.x = in_obj.max(0);
				obj.max.y = in_obj.max(1);
				obj.max.z = in_obj.max(2);

				obj.tag = id;
				objs.emplace_back(obj);
			}

			assert(ref_count > 0);
			objs.back().tag |= 0x80000000; // Tag last entry
		}
	};
};

template <>
class BvhNBboxAdapter<2> {
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

	using BvhBuilder = BasicBvhBuilder<BoundingBox, 2, CostFn>;
	using Adapter	 = BvhNBboxAdapter;
	using Node		 = typename BvhNBbox<2>::Node;
	using Obj		 = TaggedBBox;

	std::vector<Node>& nodes_;
	std::vector<Obj>& objs_;
	BvhBuilder builder_;

public:
	BvhNBboxAdapter(std::vector<Node>& nodes, std::vector<Obj>& objs)
		: nodes_(nodes)
		, objs_(objs)
	{
	}

	void build(const std::vector<IG::BoundingBox>& objs)
	{
		builder_.build(objs, NodeWriter(*this), LeafWriter(*this, objs), 2);
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
		const std::vector<IG::BoundingBox>& in_objs;

		LeafWriter(Adapter& adapter, const std::vector<IG::BoundingBox>& in_objs)
			: adapter(adapter)
			, in_objs(in_objs)
		{
		}

		template <typename RefFn>
		void operator()(int parent, int child, const BoundingBox& /*leaf_bb*/, size_t ref_count, RefFn refs)
		{
			auto& nodes = adapter.nodes_;
			auto& objs	= adapter.objs_;

			nodes[parent].child.e[child] = ~nodes.size();

			for (size_t i = 0; i < ref_count; ++i) {
				const int id	   = refs(i);
				const auto& in_obj = in_objs[id];

				TaggedBBox obj;
				obj.min.x = in_obj.min(0);
				obj.min.y = in_obj.min(1);
				obj.min.z = in_obj.min(2);

				obj.pad0 = 0;

				obj.max.x = in_obj.max(0);
				obj.max.y = in_obj.max(1);
				obj.max.z = in_obj.max(2);

				obj.tag = id;
				objs.emplace_back(obj);
			}
			assert(ref_count > 0);
			objs.back().tag |= 0x80000000; // Tag last entry
		}
	};
};

template <size_t N>
inline void build_scene_bvh(std::vector<typename BvhNBbox<N>::Node>& nodes,
							std::vector<TaggedBBox>& objs,
							std::vector<BoundingBox>& boundingBoxes)
{
	BvhNBboxAdapter<N> adapter(nodes, objs);
	adapter.build(boundingBoxes);
}
} // namespace IG