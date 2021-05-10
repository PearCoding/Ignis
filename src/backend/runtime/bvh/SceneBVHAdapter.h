#pragma once

#include "BVH.h"
#include "math/BoundingBox.h"

#include "Target.h"

// Contains implementation for NodeN
#include "generated_interface.h"

namespace IG {
struct EntityObject {
	BoundingBox BBox;
	uint32 ShapeID;
	Matrix4f Local;
};

template <>
struct ObjectAdapter<EntityObject> {
	const EntityObject& obj;

	ObjectAdapter(const EntityObject& t)
		: obj(t)
	{
	}

	inline BoundingBox computeBoundingBox() const
	{
		return obj.BBox;
	}

	inline Vector3f center() const
	{
		return (obj.BBox.max + obj.BBox.min) * (1.0f / 2.0f);
	}

	inline void computeSplit(BoundingBox& left_bb, BoundingBox& right_bb, int axis, float split) const
	{
		// Should not be used
		IG_ASSERT(false, "Compute Split should not be used when UseSpatialSplits is disabled");
		obj.BBox.computeSplit(left_bb, right_bb, axis, split);
	}
};

template <typename Adapter>
struct LeafWriterEnt1 {
	Adapter& adapter;
	const std::vector<EntityObject>& in_objs;

	LeafWriterEnt1(Adapter& adapter, const std::vector<EntityObject>& in_objs)
		: adapter(adapter)
		, in_objs(in_objs)
	{
	}

	template <typename RefFn>
	void operator()(int parent, int child, const BoundingBox& /*leaf_bb*/, size_t ref_count, RefFn refs)
	{
		//assert(ref_count > 0);

		auto& nodes = adapter.nodes_;
		auto& objs	= adapter.objs_;

		nodes[parent].child.e[child] = ~objs.size();

		for (size_t i = 0; i < ref_count; ++i) {
			const int id	   = refs(i);
			const auto& in_obj = in_objs[id];

			objs.emplace_back(EntityLeaf1{
				{ in_obj.BBox.min(0), in_obj.BBox.min(1), in_obj.BBox.min(2) },
				id,
				{ in_obj.BBox.max(0), in_obj.BBox.max(1), in_obj.BBox.max(2) },
				(int)in_obj.ShapeID,
				{ { { { in_obj.Local(0, 0), in_obj.Local(1, 0), in_obj.Local(2, 0), in_obj.Local(3, 0) },
					  { in_obj.Local(0, 1), in_obj.Local(1, 1), in_obj.Local(2, 1), in_obj.Local(3, 1) },
					  { in_obj.Local(0, 2), in_obj.Local(1, 2), in_obj.Local(2, 2), in_obj.Local(3, 2) },
					  { in_obj.Local(0, 3), in_obj.Local(1, 3), in_obj.Local(2, 3), in_obj.Local(3, 3) } } } } });
		}

		// Tag last entry
		objs.back().entity_id |= 0x80000000;
	}
};

template <size_t N>
struct BvhNEnt {
};

template <>
struct BvhNEnt<8> {
	using Node = Node8;
};

template <>
struct BvhNEnt<4> {
	using Node = Node4;
};

template <>
struct BvhNEnt<2> {
	using Node = Node2;
};

template <size_t N>
class BvhNEntAdapter {
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

	using BvhBuilder = BasicBvhBuilder<EntityObject, N, CostFn>;
	using Adapter	 = BvhNEntAdapter;
	using Node		 = typename BvhNEnt<N>::Node;
	using Obj		 = EntityLeaf1;

	friend class LeafWriterEnt1<Adapter>;

	std::vector<Node>& nodes_;
	std::vector<Obj>& objs_;
	BvhBuilder builder_;

public:
	BvhNEntAdapter(std::vector<Node>& nodes, std::vector<Obj>& objs)
		: nodes_(nodes)
		, objs_(objs)
	{
	}

	void build(const std::vector<EntityObject>& objs)
	{
		builder_.build(objs, NodeWriter(*this), LeafWriterEnt1<Adapter>(*this, objs), 1);
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

			assert(count >= 1 && count <= N);

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
};

template <>
class BvhNEntAdapter<2> {
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

	using BvhBuilder = BasicBvhBuilder<EntityObject, 2, CostFn>;
	using Adapter	 = BvhNEntAdapter;
	using Node		 = typename BvhNEnt<2>::Node;
	using Obj		 = EntityLeaf1;

	friend class LeafWriterEnt1<Adapter>;

	std::vector<Node>& nodes_;
	std::vector<Obj>& objs_;
	BvhBuilder builder_;

public:
	BvhNEntAdapter(std::vector<Node>& nodes, std::vector<Obj>& objs)
		: nodes_(nodes)
		, objs_(objs)
	{
	}

	void build(const std::vector<EntityObject>& objs)
	{
		builder_.build(objs, NodeWriter(*this), LeafWriterEnt1<Adapter>(*this, objs), 2);
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
				assert(parent >= 0 && parent < (int)nodes.size());
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
};

template <size_t N>
inline void build_scene_bvh(std::vector<typename BvhNEnt<N>::Node>& nodes,
							std::vector<EntityLeaf1>& objs,
							std::vector<EntityObject>& in_objs)
{
	BvhNEntAdapter<N> adapter(nodes, objs);
	adapter.build(in_objs);
}
} // namespace IG