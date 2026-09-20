//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The concurrent module: the lock-free containers, concurrent_cache, the
// concurrent weak containers, intern, copy_on_write, atomic.
#pragma once

#include "atomic.h"
#include "atomic_ref.h"
#include "concurrent_bounded_queue.h"
#include "concurrent_cache.h"
#include "concurrent_sorted_map.h"
#include "concurrent_priority_queue.h"
#include "concurrent_queue.h"
#include "concurrent_sorted_set.h"
#include "concurrent_stack.h"
#include "concurrent_map.h"
#include "concurrent_set.h"
#include "concurrent_weak_map.h"
#include "concurrent_weak_set.h"
#include "copy_on_write.h"
#include "intern.h"
#include "spsc_queue.h"
