//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The concurrent module: the lock-free containers, cache, the
// concurrent weak containers, intern, copy_on_write; atomic and atomic_ref
// are core's (core/atomic.h).
#pragma once

#include "bounded_queue.h"
#include "cache.h"
#include "sorted_map.h"
#include "priority_queue.h"
#include "queue.h"
#include "sorted_set.h"
#include "stack.h"
#include "map.h"
#include "set.h"
#include "weak_map.h"
#include "weak_set.h"
#include "copy_on_write.h"
#include "intern.h"
#include "spsc_queue.h"
