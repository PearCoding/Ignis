#pragma once

#include "IG_Config.h"

namespace IG {
// We do not make use of C++20, therefore we do have our own Range
template <typename T>
class Range {
public:
	class Iterator : public std::iterator<std::random_access_iterator_tag, T> {
		friend class Range;

	public:
		using difference_type = typename std::iterator<std::random_access_iterator_tag, T>::difference_type;

		inline T operator*() const { return mValue; }
		inline T operator[](const T& n) const { return mValue + n; }

		inline const Iterator& operator++()
		{
			++mValue;
			return *this;
		}

		inline Iterator operator++(int)
		{
			Iterator copy(*this);
			++mValue;
			return copy;
		}

		inline const Iterator& operator--()
		{
			--mValue;
			return *this;
		}

		inline Iterator operator--(int)
		{
			Iterator copy(*this);
			--mValue;
			return copy;
		}

		inline Iterator& operator+=(const Iterator& other)
		{
			mValue += other.mValue;
			return *this;
		}

		inline Iterator& operator+=(const T& other)
		{
			mValue += other;
			return *this;
		}

		inline Iterator& operator-=(const Iterator& other)
		{
			mValue -= other.mValue;
			return *this;
		}

		inline Iterator& operator-=(const T& other)
		{
			mValue -= other;
			return *this;
		}

		inline bool operator<(const Iterator& other) const { return mValue < other.mValue; }
		inline bool operator<=(const Iterator& other) const { return mValue <= other.mValue; }
		inline bool operator>(const Iterator& other) const { return mValue > other.mValue; }
		inline bool operator>=(const Iterator& other) const { return mValue >= other.mValue; }

		inline bool operator==(const Iterator& other) const { return mValue == other.mValue; }
		inline bool operator!=(const Iterator& other) const { return mValue != other.mValue; }

		inline Iterator operator+(const Iterator& b) const { return Iterator(mValue + *b); }
		inline Iterator operator+(const Iterator::difference_type& b) const { return Iterator(mValue + b); }
		inline friend Iterator operator+(const Iterator::difference_type& a, const Iterator& b) { return Iterator(a + *b); }
		inline difference_type operator-(const Iterator& b) const { return difference_type(mValue - *b); }
		inline Iterator operator-(const Iterator::difference_type& b) const { return Iterator(mValue - b); }
		inline friend Iterator operator-(const Iterator::difference_type& a, const Iterator& b) { return Iterator(a - *b); }

		inline Iterator()
			: mValue(0)
		{
		}

		inline explicit Iterator(const T& start)
			: mValue(start)
		{
		}

		inline Iterator(const Iterator& other) = default;
		inline Iterator(Iterator&& other)	   = default;
		inline Iterator& operator=(const Iterator& other) = default;
		inline Iterator& operator=(Iterator&& other) = default;

	private:
		T mValue;
	};

	Iterator begin() const { return mBegin; }
	Iterator end() const { return mEnd; }
	Range(const T& begin, const T& end)
		: mBegin(begin)
		, mEnd(end)
	{
	}

private:
	Iterator mBegin;
	Iterator mEnd;
};

using RangeS = Range<size_t>;
}