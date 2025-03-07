//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "root_allocator.h"

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sgcl {
    template<class T>
    using RootVector = std::vector<T, RootAllocator<T>>;

    template<class T>
    using RootList = std::list<T, RootAllocator<T>>;

    template<class T>
    using RootForwardList = std::forward_list<T, RootAllocator<T>>;

    template<class K, class C = std::less<K>>
    using RootSet = std::set<K, C, RootAllocator<K>>;

    template<class K, class C = std::less<K>>
    using RootMultiset = std::multiset<K, C, RootAllocator<K>>;

    template<class K, class T, class C = std::less<K>>
    using RootMap = std::map<K, T, C, RootAllocator<std::pair<const K, T>>>;

    template<class K, class T, class C = std::less<K>>
    using RootMultimap = std::multimap<K, T, C, RootAllocator<std::pair<const K, T>>>;

    template<class K, class H = std::hash<K>, class E = std::equal_to<K>>
    using RootUnorderedSet = std::unordered_set<K, H, E, RootAllocator<K>>;

    template<class K, class H = std::hash<K>, class E = std::equal_to<K>>
    using RootUnorderedMultiset = std::unordered_multiset<K, H, E, RootAllocator<K>>;

    template<class K, class T, class H = std::hash<K>, class E = std::equal_to<K>>
    using RootUnorderedMap = std::unordered_map<K, T, H, E, RootAllocator<std::pair<const K, T>>>;

    template<class K, class T, class H = std::hash<K>, class E = std::equal_to<K>>
    using RootUnorderedMultimap = std::unordered_multimap<K, H, E, RootAllocator<std::pair<const K, T>>>;

    template<class T>
    using RootDeque = std::deque<T, RootAllocator<T>>;

    template<class T, class Container = RootDeque<T>>
    using RootQueue = std::queue<T, Container>;

    template<class T, class Container = RootVector<T>, class Compare = std::less<typename Container::value_type>>
    using RootPriority_queue = std::priority_queue<T, Container, Compare>;

    template<class T, class Container = RootDeque<T>>
    using RootStack = std::stack<T, Container>;
}
