//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array.h"
#include "atomic.h"
#include "atomic_ref.h"
#include "collector.h"
#include "config.h"
#include "coroutine.h"
#include "deque.h"
#include "expiry_queue.h"
#include "forward_list.h"
#include "list.h"
#include "make_tracked.h"
#include "map.h"
#include "multimap.h"
#include "multiset.h"
#include "queue.h"
#include "set.h"
#include "stack.h"
#include "tracked_ptr.h"
#include "unique_ptr.h"
#include "unordered_map.h"
#include "unordered_multimap.h"
#include "unordered_multiset.h"
#include "unordered_set.h"
#include "vector.h"
#include "weak_map.h"
#include "weak_ptr.h"
#include "weak_set.h"

// over the pointers and the maker above
#include "aliases.h"
#include "any.h"
#include "expected.h"
#include "function.h"
#include "variant.h"

#include "../gc/gc.h"   // the gc namespace: the same types with gc::tracked_ptr as their word
