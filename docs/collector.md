# sgcl::collector

```cpp
#include "sgcl/sgcl.h"        // or "sgcl/collector.h"

namespace sgcl {
    class collector;
}
```

`collector` is the program's handle on the garbage collector: a class of static functions and two plain structs, with no instance. The collector itself starts with the first managed object, on a thread of its own, and runs its cycles by itself; nothing in a program has to call anything here. What the class offers is for analysis and control: forcing a cycle and waiting for it, counting and listing the live objects, reading the counters of the collector's work and the composition of the live heap by type, the ceiling on managed memory, and stopping the collector.

Three of the functions (`get_live_object_count`, `get_live_objects`, `get_type_statistics`) and `force_collect` run a full collection first, so their answers are complete: a young cycle would leave garbage among the old objects ([Generations](../README.md#generations)). They also zero the unused stack below the caller's frame first, because the stack is scanned conservatively and words left behind by dead frames would otherwise keep objects alive and distort a count taken right after a scope ([Stack roots](../README.md#stack-roots)). What they cannot clear is the caller's own frame: a raw pointer or an iterator kept there does retain its target, so a test that needs an exact count keeps its pointer-juggling code in a helper function.

## Rules

- Every function may be called from any thread, at any time, concurrently with the mutators and with each other. None of them stops a mutator; `get_live_objects()` pauses the collector while its `pause_guard` lives.
- `force_collect(true)`, `get_live_object_count()`, `get_live_objects()` and `get_type_statistics()` block the calling thread until a full cycle after the call has completed; they must not be called from a destructor of a managed object (a destructor runs on the collector's threads, inside the cycle they would wait for).
- `force_collect()` is optional in every program: the collector runs its cycles by itself. The examples and tests call it only to show or check a result at once.
- `get_live_object_count`, `get_live_objects`, `get_type_statistics`, `force_collect` and `clear_stack` are declared always-inline, so that no frame of their own lies between the caller and the area they zero.

## Members

### pause_guard

```cpp
using pause_guard = detail::Collector::PauseGuard;   // a movable RAII object
```

The first element of what `get_live_objects()` returns. While it lives, the collector is paused: it runs no cycle, so the raw pointers in the list stay valid. It is destroyed at the end of the scope that holds it (or by `reset()`), and the collector resumes. Keep it as short as the analysis needs; nothing a mutator does waits for it.

```cpp
{
    auto [guard, objects] = sgcl::collector::get_live_objects();
    // the collector is paused: every void* in `objects` is a live object
}   // the guard is destroyed here, the collector resumes
```

### get_live_object_count

```cpp
static size_t get_live_object_count();
```

Runs a full collection, waits for it and returns the number of objects it marked: every managed object reachable from a root, containers' buffers included. Zeroes the unused stack below the caller's frame first. Blocks the caller for the length of a cycle.

```cpp
size_t before = sgcl::collector::get_live_object_count();
{
    sgcl::tracked_ptr p = sgcl::make_tracked<int>(1);
    assert(sgcl::collector::get_live_object_count() == before + 1);
}
// p is gone; the count zeroes the dead frame words first
assert(sgcl::collector::get_live_object_count() == before);
```

### get_live_objects

```cpp
static std::tuple<pause_guard, std::vector<void*>> get_live_objects();
```

Runs a full collection, waits for it and returns the addresses of the objects it marked, together with a `pause_guard`. The addresses are raw pointers: the address of each managed object (what `make_tracked` returned for it) and, for a container's buffer, the start of its slot, which is the buffer's header rather than its first element. They are valid while the guard lives; the collector is paused until then. Zeroes the unused stack below the caller's frame first.

```cpp
{
    auto [guard, objects] = sgcl::collector::get_live_objects();
    for (void* object : objects) {
        std::cout << object << '\n';
    }
}   // the guard is destroyed: the collector resumes
```

### force_collect

```cpp
static bool force_collect(bool wait = false) noexcept;
```

Requests a full collection. With `wait == false` it wakes the collector and returns `true` at once; the cycle runs concurrently. With `wait == true` it returns once a full cycle that started after the call has completed and found nothing more to remove: the current cycle may be half done, and a destructor that stores a pointer keeps its target for one more cycle, so several cycles may run (a bounded number, since a mutator that keeps allocating produces garbage forever). It returns `false` only if the collector is terminating and no such cycle will come. Zeroes the unused stack below the caller's frame first. Optional: the collector runs its cycles by itself, and a program that calls it in a loop only wastes CPU on cycles that would have run anyway.

```cpp
sgcl::weak_ptr<Item> weak;
{
    sgcl::tracked_ptr item = sgcl::make_tracked<Item>();
    weak = item;
}
sgcl::collector::force_collect(true);   // optional, for the demonstration only: the next cycle clears it anyway
assert(weak.expired());
```

### clear_stack

```cpp
static void clear_stack(size_t bytes = config::StackClearSize) noexcept;
```

Zeroes `bytes` of the unused stack below the caller's frame (`SIZE_MAX` for the whole unused stack), never closer than `config::StackGuardMargin` to the end of the thread's stack and never pages the stack has not touched. Objects referenced only by words left behind in dead frames become collectable. Called by `force_collect()` and the counting functions; on its own it is for a long-lived loop that wants a stale root gone before the next cycle rather than before the next count.

```cpp
process_batch();                           // deep frames, now dead, may hold stale pointers
sgcl::collector::clear_stack();            // 64 KB below this frame zeroed
sgcl::collector::clear_stack(SIZE_MAX);    // or the whole unused stack
```

### terminate

```cpp
static void terminate() noexcept;
```

Stops the collector: the current cycle finishes, cycles run until nothing dies any more (the objects still reachable are not destroyed), the helper threads and the collector thread exit, and the call returns. Optional: a program may simply end. After it no cycle runs: objects are still allocated and destroyed through `unique_ptr`, tracked garbage stays until the process exits, and `force_collect(true)` returns `false`.

```cpp
int main() {
    run();                          // the program's work
    sgcl::collector::terminate();   // the collector's threads are gone from here on
    return 0;
}
```

### statistics, get_statistics, phase_names

```cpp
struct statistics {
    size_t cycles;              // completed since the start
    size_t full_cycles;         // of which full (all of them unless generational)
    size_t live_objects;        // objects marked by the last cycle
    size_t live_bytes;          // managed memory in use by the allocators now
    size_t committed_bytes;     // managed memory committed now
    double last_cycle_ms;       // wall time of the last cycle
    unsigned helper_threads;    // helper threads started so far
    unsigned last_helpers_used; // of which the last cycle used
    bool helpers_enabled;       // the helpers are on for the next cycle
    double phases_ms[8];        // the last cycle's phases: registration, states, roots, marking, updated states, sweep, page release, trim
};

static constexpr const char* phase_names[8] = {"registration", "states", "roots", "marking", "updated", "sweep", "release", "trim"};

static statistics get_statistics() noexcept;
```

Counters of the collector's work, read without stopping it and without waiting for anything: every field is a relaxed load of a counter the collector thread stores at the end of a cycle. The values describe the last cycle that completed, except `committed_bytes` and `live_bytes`, which are read now; `live_bytes` counts the pages in use by the allocators, garbage not yet swept included, so it is at least what the live objects take. `phases_ms[i]` is the wall time of phase `i` of the last cycle, named by `phase_names[i]`; the eight add up to `last_cycle_ms`, give or take the clock. Before the first cycle every counter is zero.

```cpp
auto s = sgcl::collector::get_statistics();
std::cout << s.cycles << " cycles (" << s.full_cycles << " full), "
          << s.live_objects << " objects, " << s.live_bytes / 1048576 << " MB live, "
          << s.committed_bytes / 1048576 << " MB committed, last cycle "
          << s.last_cycle_ms << " ms with " << s.last_helpers_used << " helpers\n";
for (int i = 0; i < 8; ++i) {
    std::cout << sgcl::collector::phase_names[i] << ' ' << s.phases_ms[i] << " ms\n";
}
```

### type_statistics, get_type_statistics

```cpp
struct type_statistics {
    const std::type_info* type;   // the object's type, or the array type of a buffer (typeid(T[]))
    bool buffers;                 // true for the buffers of the containers
    size_t object_size;           // bytes of one object, or of one element of a buffer
    size_t live_objects;          // objects (or buffers) of this type after the cycle
    size_t live_bytes;            // their bytes: the slots they occupy
    size_t pages;                 // pages of this type's pools; 0 for buffers
};

static std::vector<type_statistics> get_type_statistics();
```

The live objects by type after a full cycle: what a heap that grows is made of. Objects are listed by their type; the buffers of the containers (`vector`, `array<T>`, the maps of `deque`, the buckets of the hash tables) by their array type, `typeid(T[])` for elements `T`, with `buffers == true`, the slot they occupy as their bytes and no pages, since the pages of buffers belong to size classes rather than to a type. `object_size` is the slot size, at least `sizeof(T)`. Sorted by `live_bytes`, descending, then by `live_objects`. Like `get_live_objects()`: a full cycle runs first, the caller's dead frames are zeroed, the caller waits for the cycle.

```cpp
for (auto& t : sgcl::collector::get_type_statistics()) {
    std::cout << (t.buffers ? "buffers of " : "") << t.type->name() << ": "
              << t.live_objects << " x " << t.object_size << " B = " << t.live_bytes << " B";
    if (!t.buffers) {
        std::cout << ", " << t.pages << " pages";
    }
    std::cout << '\n';
}
```

### get_committed_memory

```cpp
static size_t get_committed_memory() noexcept;
```

Bytes of managed memory committed right now: the part of the heap's reserved range backed by physical memory, in 2 MB chunks, free chunks kept for reuse included ([Memory](../README.md#memory)). The reservation itself (the process's virtual size) is not counted.

```cpp
std::cout << sgcl::collector::get_committed_memory() / 1048576 << " MB committed\n";
```

### get_memory_limit, set_memory_limit

```cpp
static size_t get_memory_limit() noexcept;
static void set_memory_limit(size_t bytes) noexcept;
```

The ceiling on committed managed memory, in bytes: by default 90% of the cgroup memory limit on Linux, or of the physical memory elsewhere (`config::HeapLimitPercent`). Above 75% of it (`config::HeapPressurePercent`) the collector cycles every 100 ms and returns every free chunk to the system at once. When an allocation would cross it, the allocation first forces a full collection and waits for it; if that does not free enough, it throws `std::bad_alloc` instead of letting the process run into the OOM killer. `set_memory_limit(0)` disables the ceiling. The setting takes effect for the next chunk committed; it does not shrink what is committed already.

```cpp
auto limit = sgcl::collector::get_memory_limit();           // the default ceiling
sgcl::collector::set_memory_limit(size_t(4) << 30);          // 4 GB
try {
    sgcl::vector<int> huge(size_t(2) << 30);                 // 8 GB of int: over the ceiling
} catch (const std::bad_alloc&) {
    // a full collection ran first; not enough was free
}
sgcl::collector::set_memory_limit(limit);                    // back to the default
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

struct Node {
    sgcl::tracked_ptr<Node> next;
    int value;
};

// The stack is scanned conservatively: the pointers juggled here stay in a
// frame of their own, which the counts below zero before they count.
static void build_and_drop(size_t count) {
    sgcl::tracked_ptr<Node> head;
    for (size_t i = 0; i < count; ++i) {
        sgcl::tracked_ptr node = sgcl::make_tracked<Node>();
        node->next = head;
        node->value = int(i);
        head = node;
    }
    std::cout << "with the list: " << sgcl::collector::get_live_object_count() << " live objects\n";
}   // head is gone: the whole list is garbage

int main() {
    size_t before = sgcl::collector::get_live_object_count();
    build_and_drop(1000);
    // the count runs a full cycle first and zeroes the frames build_and_drop left behind
    std::cout << "after the list: " << sgcl::collector::get_live_object_count() - before << " new live objects\n";

    sgcl::vector<sgcl::tracked_ptr<Node>> kept;
    for (int i = 0; i < 10; ++i) {
        kept.push_back(sgcl::make_tracked<Node>());
    }
    // what the live heap is made of, by type: the ten nodes and the vector's buffer
    for (auto& t : sgcl::collector::get_type_statistics()) {
        if (*t.type == typeid(Node) || *t.type == typeid(sgcl::tracked_ptr<Node>[])) {
            std::cout << (t.buffers ? "buffers of " : "") << t.type->name() << ": "
                      << t.live_objects << " x " << t.object_size << " B\n";
        }
    }

    sgcl::collector::force_collect(true);   // optional, for the demonstration only: the collector runs its cycles by itself
    auto s = sgcl::collector::get_statistics();
    std::cout << s.cycles << " cycles, " << s.live_objects << " live objects, last cycle "
              << s.last_cycle_ms << " ms, " << sgcl::collector::get_committed_memory() / 1048576
              << " MB committed of a " << sgcl::collector::get_memory_limit() / 1048576 << " MB ceiling\n";
    return 0;
}
```

## See also

- [config](config.md): the constants behind the stack clearing, the memory ceiling, the generations and the helpers.
- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [weak_ptr](weak_ptr.md), [expiry_queue](expiry_queue.md).
- README: [Methods useful for state analysis](../README.md#methods-useful-for-state-analysis), [Memory](../README.md#memory), [Generations](../README.md#generations), [Stack roots](../README.md#stack-roots), [Threads](../README.md#threads), [The rules](../README.md#the-rules).
- `tests/statistics.cpp`, `tests/heap.cpp`: the counters and the memory ceiling exercised.
