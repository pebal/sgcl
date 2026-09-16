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
- In the child of a `fork()` the collector does not run (the child has no thread but the one that forked): the child may read managed objects and `exec` or exit; `force_collect`, `terminate` and a managed allocation that needs a page terminate the child with a message ([Threads](../README.md#threads)).
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
sgcl::collector::force_collect(true);     // optional, for the demonstration only: the next cycle clears it anyway
assert(weak.expired());
```

### clear_stack

```cpp
static void clear_stack(size_t bytes = config::StackClearSize) noexcept;
```

Zeroes `bytes` of the unused stack below the caller's frame (`SIZE_MAX` for the whole unused stack), never closer than `config::StackGuardMargin` to the end of the thread's stack and never pages the stack has not touched. Objects referenced only by words left behind in dead frames become collectable. Called by `force_collect()` and the counting functions; on its own it is for a long-lived loop that wants a stale root gone before the next cycle rather than before the next count.

```cpp
process_batch();                           // deep frames, now dead, may hold stale pointers
sgcl::collector::clear_stack();              // 64 KB below this frame zeroed
sgcl::collector::clear_stack(SIZE_MAX);      // or the whole unused stack
```

### terminate

```cpp
static void terminate() noexcept;
```

Stops the collector: the current cycle finishes, cycles run until nothing dies any more (the objects still reachable are not destroyed), the helper threads and the collector thread exit, and the call returns. Optional: a program may simply end. After it no cycle runs: objects are still allocated and destroyed through `unique_ptr`, tracked garbage stays until the process exits, and `force_collect(true)` returns `false`.

```cpp
int main() {
    run();                          // the program's work
    sgcl::collector::terminate();     // the collector's threads are gone from here on
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
auto limit = sgcl::collector::get_memory_limit();             // the default ceiling
sgcl::collector::set_memory_limit(size_t(4) << 30);            // 4 GB
try {
    sgcl::vector<int> huge(size_t(2) << 30);                   // 8 GB of int: over the ceiling
} catch (const std::bad_alloc&) {
    // a full collection ran first; not enough was free
}
sgcl::collector::set_memory_limit(limit);                      // back to the default
```

### referrer, get_referrers, get_path_to_root, explain

```cpp
struct referrer {
    enum class kind { object, buffer, stack, cell, unique, weak };
    kind from;
    const void* holder;           // the object, buffer or block that holds the word; the word itself on a stack
    const std::type_info* type;   // the holder's type; a buffer's element type (typeid(T[])); null for a stack word
    size_t offset;                // the word's byte offset in the holder
    std::thread::id thread;       // a stack word: the thread whose stack it is on
};

struct retained {
    size_t objects;
    size_t bytes;
};

static std::tuple<pause_guard, std::vector<referrer>> get_referrers(const void* p);
static std::tuple<pause_guard, std::vector<referrer>> get_path_to_root(const void* p);
static retained get_retained(const void* p);
static void explain(const void* p, std::ostream& out);
```

What holds an object. Both run a full cycle first and keep the collector paused while the `pause_guard` lives, as `get_live_objects()` does, so that the live objects are exactly the marked ones and no page moves under the walk (the mutators run on; a word is read as the scan reads it). `p` may point into the object. One guard at a time: a call made while a guard lives waits for a cycle the paused collector cannot run.

`get_referrers` lists every word that points at the object: the members of objects (`object`, with the holder's type and the word's offset), the elements of buffers (`buffer`, the element type as `typeid(T[])`, the offset from the buffer's start, header included), the cells of [`root_ptr`](root_ptr.md)s in unmanaged memory (`cell`: the block and the cell's offset; the `root_ptr` that owns the cell is not known to the collector), the words of every thread's stack (`stack`: the word's address and the thread's id; on the calling thread, the frames above the call, so a local that holds the object is listed), the object itself when a `unique_ptr` owns it (`unique`), and the cells of `weak_ptr`s (`weak`), which hold nothing.

`get_path_to_root` is a chain from the object up to a root: `[0]` holds the object, `[1]` holds that holder, and so on to a root: an object a `unique_ptr` owns, a block of cells (a `unique` link with `typeid(detail::CellBlock)`, a root by its state), a word on a stack. A search from the roots down, breadth first, the roots by state first, then the other threads' stacks, and the calling thread's frames above the call only when nothing else reaches the object: the caller holds the pointer it asks about and asks what else does, so a chain that ends on its own stack says that nothing else does. The search does not go through a weak cell: a `weak_ptr` holds nothing, so no chain leads through one (`get_referrers` still lists it). Empty: `p` is not into a live managed object, or only the frames of the call hold it.

`get_retained` is what dies with the object: the objects reachable from it and from nowhere else, itself included, and their bytes, the slots they occupy (a container's buffer at the slot of its size class). What is shared with another root stays out; a weak pointer holds nothing, so what is reachable only through a `weak_ptr` is not retained by its holder. A full cycle first and the collector paused for the walk, as above, the guard released on return; `{0, 0}` for a pointer that is not into a live managed object.

`explain` writes the chain as text, one line per link, and what the object retains; or why there is no chain.

```cpp
sgcl::unique_ptr<Node> head = sgcl::make_tracked<Node>();
head->next = sgcl::make_tracked<Node>();
head->next->leaf = sgcl::make_tracked<Leaf>();
sgcl::collector::explain(head->next->leaf.get(), std::cout);
// 0x100... is held by
//   a Node at 0x100..., the word at byte 0
//   a Node at 0x100..., the word at byte 8
//   a unique_ptr: the Node at 0x100... is its object
// and keeps alive 1 object, 4 bytes, itself included
```

### stepper

```cpp
class stepper {
public:
    enum class phase { start, flipped, registered, roots, marked, swept, released };
    explicit stepper(bool full = true);
    ~stepper();
    phase step() noexcept;                 // one gate: the phase the collector stands at
    phase advance_to(phase p) noexcept;    // gates until the collector stands at the next `p`
    void finish_cycle() noexcept;          // to `released`
    void full(bool full) noexcept;         // the kind of the cycles from the next one on
    void helpers(unsigned n) noexcept;     // this many helper threads for every pass, whatever the work; 0 the policy
    phase current() const noexcept;
};
```

The collector one gate at a time, for the tests of the engine. While a `stepper` exists no cycle runs by itself: the collector stands at a gate, a boundary between the phases of a cycle, until `step()` lets it through to the next, and the calling thread, the mutator of the test, does its work in between, in the window a race would otherwise have to land in by luck. The gates: `start` (a cycle about to begin), `flipped` (the epoch flipped, nothing registered yet: an object made now stays unregistered for this cycle), `registered` (the pages, objects and threads of before the flip registered, the blocks of cells released), `roots` (the stacks scanned, the dirty pages of a young cycle traced), `marked` (the marking converged, the weak cells cleared), `swept` (the garbage destroyed and freed), `released` (the empty pages back in the heap: the cycle is over). The cycles are full unless the stepper is made with `full = false` or `full(false)` is called; `settle`-like sequences in the tests run two full cycles to clear what the previous test left. A cycle in flight when the stepper is made runs to its end unstepped; the destructor lets the collector run on by itself.

`helpers(n)` forces `n` helper threads on every pass of the cycles from here on, however small the heap: the parallel marking with its work stealing, the sweep, the stack scan and the states pass, which the policy starts only from a million objects or 256 pages. The library's own scenarios run twice, alone and with two helpers.

The stepper's thread must not wait for the collector between gates: `force_collect`, `get_live_object_count`, `get_live_objects` and `get_type_statistics` would deadlock. `get_statistics()` reads the counters of the last completed cycle and is fine. What the tests of the engine assert with it (`tests/stepping.cpp`): an object made after the flip and released into an old object is neither swept this cycle nor lost by the next; one made before the flip and released after the roots were traced is reachable by the state of its release alone; a store into an old, marked object after its page was traced is found by the next young cycle through the card; a `weak_ptr` locked before the weak phase holds its object through the cycle and reads null after it; a pointer loaded from an `atomic` after the stacks were scanned, its only other reference dropped, is a root by the state of the copy; a thread exiting before the scan takes its objects with it, one exiting after keeps them for the cycle; an object watched by an `expiry_queue` is kept for the drain; a block of cells is freed by the cycle after the one that saw its last cell go.

```cpp
sgcl::collector::stepper s(false);                     // young cycles
sgcl::tracked_ptr holder = sgcl::make_tracked<Node>();
s.finish_cycle();                                    // holder is old and marked
s.advance_to(sgcl::collector::stepper::phase::roots);  // the stacks scanned, the dirty pages traced
holder->next = sgcl::make_tracked<Node>();             // stored into an old object after the trace: the card
s.finish_cycle();                                    // not swept: made after the flip
s.finish_cycle();                                    // registered now, found through the card
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

    sgcl::collector::force_collect(true);     // optional, for the demonstration only: the collector runs its cycles by itself
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
