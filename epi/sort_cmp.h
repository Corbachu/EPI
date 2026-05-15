//----------------------------------------------------------------------------
//  EPI Sort & Compare Utilities
//----------------------------------------------------------------------------
//
//  Copyright (c) 2024-2026  The EDGE Team.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//----------------------------------------------------------------------------
//
// Fast, modern replacements for qsort / sort-compare-function patterns.
// All helpers live in the epi namespace and lean on C++20 facilities where
// available (std::sort, std::stable_sort, std::lower_bound, etc.) while
// exposing a concise, game-friendly API.
//
// Highlights:
//  • SortAscending / SortDescending   – sort any RandomAccess range
//  • StableSort                        – sort preserving equal-element order
//  • BinarySearch / LowerBound / UpperBound
//  • Comparators for common game types (int, float, C-strings, pointers)
//  • IndirectSort  – sort an index array without moving the data
//  • QSortCompat   – thin C-style qsort wrapper (for legacy interop)
//
//----------------------------------------------------------------------------

#ifndef __EPI_SORT_CMP_H__
#define __EPI_SORT_CMP_H__

#include <algorithm>
#include <functional>
#include <numeric>
#include <string>
#include <cstring>
#if !defined(_WIN32)
#include <strings.h>
#endif
#include <iterator>

namespace epi
{

//----------------------------------------------------------------------------
// Basic ascending / descending sort
//----------------------------------------------------------------------------

// Sort any container or iterator range in ascending order.
template<typename Iter>
inline void SortAscending(Iter first, Iter last)
{
    std::sort(first, last);
}

// Sort using a custom comparator (ascending by default convention).
template<typename Iter, typename Cmp>
inline void SortAscending(Iter first, Iter last, Cmp cmp)
{
    std::sort(first, last, cmp);
}

// Sort a range in descending order.
template<typename Iter>
inline void SortDescending(Iter first, Iter last)
{
    std::sort(first, last, [](const auto& a, const auto& b){ return a > b; });
}

template<typename Iter, typename Cmp>
inline void SortDescending(Iter first, Iter last, Cmp cmp)
{
    // Invert the comparator so that greater elements come first.
    std::sort(first, last, [&cmp](const auto& a, const auto& b){ return cmp(b, a); });
}

// Stable sort – preserves the relative order of equivalent elements.
template<typename Iter>
inline void StableSort(Iter first, Iter last)
{
    std::stable_sort(first, last);
}

template<typename Iter, typename Cmp>
inline void StableSort(Iter first, Iter last, Cmp cmp)
{
    std::stable_sort(first, last, cmp);
}

//----------------------------------------------------------------------------
// Partial / selection sorts
//----------------------------------------------------------------------------

// Rearrange [first, last) so that the n-th element is the value that would
// be there in a fully sorted range.  Elements before it are ≤ it; elements
// after are ≥ it.  O(n) on average.
template<typename Iter>
inline void NthElement(Iter first, Iter nth, Iter last)
{
    std::nth_element(first, nth, last);
}

template<typename Iter, typename Cmp>
inline void NthElement(Iter first, Iter nth, Iter last, Cmp cmp)
{
    std::nth_element(first, nth, last, cmp);
}

// Sort only the first count elements (the "top count" items).
template<typename Iter>
inline void PartialSort(Iter first, Iter middle, Iter last)
{
    std::partial_sort(first, middle, last);
}

template<typename Iter, typename Cmp>
inline void PartialSort(Iter first, Iter middle, Iter last, Cmp cmp)
{
    std::partial_sort(first, middle, last, cmp);
}

//----------------------------------------------------------------------------
// Binary search helpers (range must be sorted first)
//----------------------------------------------------------------------------

template<typename Iter, typename T>
inline bool BinarySearch(Iter first, Iter last, const T& value)
{
    return std::binary_search(first, last, value);
}

template<typename Iter, typename T, typename Cmp>
inline bool BinarySearch(Iter first, Iter last, const T& value, Cmp cmp)
{
    return std::binary_search(first, last, value, cmp);
}

// Returns iterator to first element ≥ value.
template<typename Iter, typename T>
inline Iter LowerBound(Iter first, Iter last, const T& value)
{
    return std::lower_bound(first, last, value);
}

template<typename Iter, typename T, typename Cmp>
inline Iter LowerBound(Iter first, Iter last, const T& value, Cmp cmp)
{
    return std::lower_bound(first, last, value, cmp);
}

// Returns iterator to first element > value.
template<typename Iter, typename T>
inline Iter UpperBound(Iter first, Iter last, const T& value)
{
    return std::upper_bound(first, last, value);
}

template<typename Iter, typename T, typename Cmp>
inline Iter UpperBound(Iter first, Iter last, const T& value, Cmp cmp)
{
    return std::upper_bound(first, last, value, cmp);
}

//----------------------------------------------------------------------------
// Indirect sort – sort an index array [0, N) by data values without
// moving the underlying data.  Useful for parallel arrays.
//
// Usage:
//   std::vector<float> costs = { … };
//   std::vector<int>   order = IndirectSort(costs.begin(), costs.end());
//   // order[0] is index of smallest cost, etc.
//----------------------------------------------------------------------------
template<typename Iter>
inline std::vector<int> IndirectSort(Iter first, Iter last)
{
    int n = static_cast<int>(std::distance(first, last));
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&first](int a, int b){ return *(first + a) < *(first + b); });
    return idx;
}

template<typename Iter, typename Cmp>
inline std::vector<int> IndirectSort(Iter first, Iter last, Cmp cmp)
{
    int n = static_cast<int>(std::distance(first, last));
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&first, &cmp](int a, int b){ return cmp(*(first + a), *(first + b)); });
    return idx;
}

//----------------------------------------------------------------------------
// Named comparator functors for common game types
//----------------------------------------------------------------------------

// Compare two C-style strings lexicographically.
struct CStrLess
{
    bool operator()(const char* a, const char* b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

struct CStrGreater
{
    bool operator()(const char* a, const char* b) const
    {
        return std::strcmp(a, b) > 0;
    }
};

// Case-insensitive string compare.
struct CStrILess
{
    bool operator()(const char* a, const char* b) const
    {
#if defined(_WIN32)
        return _stricmp(a, b) < 0;
#else
        return strcasecmp(a, b) < 0;
#endif
    }
};

// Compare by dereferenced pointer value (for sorting pointer arrays).
template<typename T>
struct PtrLess
{
    bool operator()(const T* a, const T* b) const
    {
        return *a < *b;
    }
};

template<typename T>
struct PtrGreater
{
    bool operator()(const T* a, const T* b) const
    {
        return *a > *b;
    }
};

// Compare floats with tolerance (useful for de-duplication steps before sort).
struct FloatNearEqual
{
    float eps;
    explicit FloatNearEqual(float epsilon = 1e-6f) : eps(epsilon) {}
    bool operator()(float a, float b) const
    {
        float d = a - b;
        return (d < 0 ? -d : d) < eps;
    }
};

//----------------------------------------------------------------------------
// Key-extractor sort helpers
//----------------------------------------------------------------------------

// Sort by a member/key extracted from each element.
// Example: SortBy(vec.begin(), vec.end(), [](auto& e){ return e.priority; });
template<typename Iter, typename KeyFn>
inline void SortBy(Iter first, Iter last, KeyFn key_fn)
{
    std::sort(first, last,
              [&key_fn](const auto& a, const auto& b)
              { return key_fn(a) < key_fn(b); });
}

template<typename Iter, typename KeyFn>
inline void SortByDescending(Iter first, Iter last, KeyFn key_fn)
{
    std::sort(first, last,
              [&key_fn](const auto& a, const auto& b)
              { return key_fn(a) > key_fn(b); });
}

//----------------------------------------------------------------------------
// Remove-duplicates helpers (range must be sorted first)
//----------------------------------------------------------------------------

template<typename Iter>
inline Iter UniqueSorted(Iter first, Iter last)
{
    return std::unique(first, last);
}

template<typename Iter, typename BinPred>
inline Iter UniqueSorted(Iter first, Iter last, BinPred pred)
{
    return std::unique(first, last, pred);
}

//----------------------------------------------------------------------------
// C qsort-compatible wrapper (for legacy code / C callbacks)
//----------------------------------------------------------------------------

// QSortCompat lets you call std::sort using a traditional C compare function.
// compare(a, b) should return < 0, 0, or > 0.
template<typename T>
inline void QSortCompat(T* array, int count, int (*compare)(const void*, const void*))
{
    std::sort(array, array + count,
              [compare](const T& a, const T& b)
              { return compare(&a, &b) < 0; });
}

} // namespace epi

#endif /* __EPI_SORT_CMP_H__ */

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab
