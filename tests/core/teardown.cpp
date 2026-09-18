//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The library used after the main thread's Thread is gone. The Thread is a
// thread_local (thread.h: register_thread), destroyed before the static
// destructors; the collector then deletes the thread's Data and its
// PageAllocator, while current_thread_ptr stays set. A static destructor
// that allocates (a global unique_ptr's object, as in
// docs/sgcl/core/unique_ptr.md: the registry) then runs on freed memory.
//
// teardown_test: a global unique_ptr<Node> whose ~Node calls make_tracked.
// teardown_thread_local_test (TEARDOWN_THREAD_LOCAL): a thread_local
// object registered before the Thread (its first use is before the first
// allocation) whose destructor allocates: destroyed after the Thread.
#include "sgcl/sgcl.h"

#include <cstdio>

using namespace sgcl;

struct Late {
    int value = 0;
};

struct Node {
    tracked_ptr<Node> next;
    int id = 0;

    ~Node() {
        std::fprintf(stderr, "~Node %d: copying a tracked_ptr and allocating\n", id);
        tracked_ptr<Node> copy = next;   // a copy: the barrier's thread data
        (void)copy;
        tracked_ptr<Late> late = make_tracked<Late>();   // the thread's allocator for Late, made now
        late->value = id;
        std::fprintf(stderr, "~Node %d: allocated %p\n", id, (void*)late.get());
    }
};

#if defined(TEARDOWN_THREAD_LOCAL)
struct AtThreadExit {
    ~AtThreadExit() {
        std::fprintf(stderr, "thread_local destructor: allocating\n");
        tracked_ptr<Late> late = make_tracked<Late>();
        late->value = 1;
        std::fprintf(stderr, "thread_local destructor: allocated %p\n", (void*)late.get());
    }
};

int main() {
    thread_local AtThreadExit at_exit;   // registered now, before the Thread: destroyed after it
    (void)at_exit;
    tracked_ptr<Node> node = make_tracked<Node>();
    node->id = 1;
    collector::force_collect(true);
    std::fprintf(stderr, "main returns\n");
    return 0;
}
#else
// The global root of docs/sgcl/core/unique_ptr.md, destroyed after main
// with the other statics
static unique_ptr<Node> registry = make_tracked<Node>();

int main() {
    registry->id = 1;
    registry->next = make_tracked<Node>();
    registry->next->id = 2;
    collector::force_collect(true);
    std::fprintf(stderr, "main returns\n");
    return 0;
}
#endif
