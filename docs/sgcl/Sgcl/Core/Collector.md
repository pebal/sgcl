# Sgcl::Collector

```cpp
#include "sgcl/Sgcl/Core/Collector.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    struct Collector;
}
```

The same class in the `sgcl` interface: [collector](../../core/collector.md).

`Collector` is the program's handle on the garbage collector: a class of static functions and a few plain structs, with no instance. The collector itself starts with the first managed object, on a thread of its own, and runs its cycles by itself; nothing in a program has to call anything here. What the class offers is for analysis and control: forcing a cycle and waiting for it, counting and listing the live objects, reading the counters of the collector's work and the composition of the live heap by type, the ceiling on managed memory, and stopping the collector.

Three of the functions (`LiveObjectCount`, `LiveObjects`, `GetTypeStatistics`) and `Collect` run a full collection first, so their answers are complete: a young cycle would leave garbage among the old objects ([Generations](../../../garbage_collector/overview.md#generations)). They also zero the unused stack below the caller's frame first, because the stack is scanned conservatively and words left behind by dead frames would otherwise keep objects alive and distort a count taken right after a scope ([Stack roots](../../../garbage_collector/overview.md#stack-roots)). What they cannot clear is the caller's own frame: a raw pointer or an iterator kept there does retain its target, so a test that needs an exact count keeps its pointer-juggling code in a helper function.

## Rules

- Every function may be called from any thread, at any time, concurrently with the mutators and with each other. None of them stops a mutator; `LiveObjects()` pauses the collector while its `PauseGuard` lives.
- `Collect(true)`, `LiveObjectCount()`, `LiveObjects()` and `GetTypeStatistics()` block the calling thread until a full cycle after the call has completed; they must not be called from a destructor of a managed object (a destructor runs on the collector's threads, inside the cycle they would wait for).
- `Collect()` is optional in every program: the collector runs its cycles by itself. The examples and tests call it only to show or check a result at once.
- In the child of a `fork()` the collector does not run (the child has no thread but the one that forked): the child may read managed objects and `exec` or exit; `Collect`, `Terminate` and a managed allocation that needs a page terminate the child with a message ([Threads](../../async/README.md#threads)).
- `LiveObjectCount`, `LiveObjects`, `GetTypeStatistics`, `Collect` and `ClearStack` are declared always-inline, so that no frame of their own lies between the caller and the area they zero.

## Members

### PauseGuard

```cpp
class PauseGuard {                                 // a movable RAII object
public:
    void Reset() noexcept;                         // the pause ended early
    explicit operator bool() const noexcept;
};
```

The first element of what `LiveObjects()` returns. While it lives, the collector is paused: it runs no cycle, so the raw pointers in the list stay valid. It is destroyed at the end of the scope that holds it (or by `Reset()`), and the collector resumes. Keep it as short as the analysis needs; nothing a mutator does waits for it.

```cpp
{
    auto [guard, objects] = Collector::LiveObjects();
    // the collector is paused: every void* in `objects` is a live object
}   // the guard is destroyed here, the collector resumes
```

### LiveObjectCount

```cpp
static size_t LiveObjectCount();
```

Runs a full collection, waits for it and returns the number of objects it marked: every managed object reachable from a root, containers' buffers included. Zeroes the unused stack below the caller's frame first. Blocks the caller for the length of a cycle.

```cpp
size_t before = Collector::LiveObjectCount();
{
    Ptr p = Make<int>(1);
    assert(Collector::LiveObjectCount() == before + 1);
}
// p is gone; the count zeroes the dead frame words first
assert(Collector::LiveObjectCount() == before);
```

### LiveObjects

```cpp
static std::tuple<PauseGuard, std::vector<void*>> LiveObjects();
```

Runs a full collection, waits for it and returns the addresses of the objects it marked, together with a `PauseGuard`. The addresses are raw pointers: the address of each managed object (what `Make` returned for it) and, for a container's buffer, the start of its slot, which is the buffer's header rather than its first element. They are valid while the guard lives; the collector is paused until then. Zeroes the unused stack below the caller's frame first.

```cpp
{
    auto [guard, objects] = Collector::LiveObjects();
    for (void* object : objects) {
        std::cout << object << '\n';
    }
}   // the guard is destroyed: the collector resumes
```

### Collect

```cpp
static bool Collect(bool wait = false) noexcept;
```

Requests a full collection. With `wait == false` it wakes the collector and returns `true` at once; the cycle runs concurrently. With `wait == true` it returns once a full cycle that started after the call has completed and found nothing more to remove: the current cycle may be half done, and a destructor that stores a pointer keeps its target for one more cycle, so several cycles may run (a bounded number, since a mutator that keeps allocating produces garbage forever). It returns `false` only if the collector is terminating and no such cycle will come. Zeroes the unused stack below the caller's frame first. Optional: the collector runs its cycles by itself, and a program that calls it in a loop only wastes CPU on cycles that would have run anyway.

```cpp
struct Item {};
WeakPtr<Item> weak;
{
    Ptr item = Make<Item>();
    weak = item;
}
Collector::Collect(true);        // optional, for the demonstration only: the next cycle clears it anyway
assert(weak.IsExpired());
```

### ClearStack

```cpp
static void ClearStack(size_t bytes = config::StackClearSize) noexcept;
```

Zeroes `bytes` of the unused stack below the caller's frame (`SIZE_MAX` for the whole unused stack), never closer than `config::StackGuardMargin` to the end of the thread's stack and never pages the stack has not touched. Objects referenced only by words left behind in dead frames become collectable. Called by `Collect()` and the counting functions; on its own it is for a long-lived loop that wants a stale root gone before the next cycle rather than before the next count.

```cpp
auto processBatch = [] {};
processBatch();                            // deep frames, now dead, may hold stale pointers
Collector::ClearStack();          // 64 KB below this frame zeroed
Collector::ClearStack(SIZE_MAX);  // or the whole unused stack
```

### Terminate

```cpp
static void Terminate() noexcept;
```

Stops the collector: the current cycle finishes, cycles run until nothing dies any more (the objects still reachable are not destroyed), the helper threads and the collector thread exit, and the call returns. Optional: a program may simply end: the main thread's exit stops the collector the same way, before the static destructors run, and those may still use the library (a global `UniquePtr`'s object making objects in its destructor), since the main thread's registration is never undone. After it no cycle runs: objects are still allocated and destroyed through `UniquePtr`, tracked garbage stays until the process exits, and `Collect(true)` returns `false`.

```cpp
#include "sgcl/Sgcl/Sgcl.h"

void Run() {}

int main() {
    Run();                             // the program's work
    Collector::Terminate();   // the collector's threads are gone from here on
    return 0;
}
```

### Statistics, GetStatistics, PhaseNames

```cpp
struct Statistics {
    size_t Cycles;              // completed since the start
    size_t FullCycles;          // of which full (all of them unless generational)
    size_t LiveObjects;         // objects marked by the last cycle
    size_t LiveBytes;           // managed memory in use by the allocators now
    size_t CommittedBytes;      // managed memory committed now
    double LastCycleMs;         // wall time of the last cycle
    unsigned HelperThreads;     // helper threads started so far
    unsigned LastHelpersUsed;   // of which the last cycle used
    bool HelpersEnabled;        // the helpers are on for the next cycle
    double PhasesMs[8];         // the last cycle's phases: registration, states, roots, marking, updated states, sweep, page release, trim
};

static constexpr const char* const* PhaseNames;   // {"registration", "states", "roots", "marking", "updated", "sweep", "release", "trim"}

static Statistics GetStatistics() noexcept;
```

Counters of the collector's work, read without stopping it and without waiting for anything: every field is a relaxed load of a counter the collector thread stores at the end of a cycle. The values describe the last cycle that completed, except `CommittedBytes` and `LiveBytes`, which are read now; `LiveBytes` counts the pages in use by the allocators, garbage not yet swept included, so it is at least what the live objects take. `PhasesMs[i]` is the wall time of phase `i` of the last cycle, named by `PhaseNames[i]`; the eight add up to `LastCycleMs`, give or take the clock. Before the first cycle every counter is zero.

```cpp
auto s = Collector::GetStatistics();
std::cout << s.Cycles << " cycles (" << s.FullCycles << " full), "
          << s.LiveObjects << " objects, " << s.LiveBytes / 1048576 << " MB live, "
          << s.CommittedBytes / 1048576 << " MB committed, last cycle "
          << s.LastCycleMs << " ms with " << s.LastHelpersUsed << " helpers\n";
for (int i : Range(8)) {
    std::cout << Collector::PhaseNames[i] << ' ' << s.PhasesMs[i] << " ms\n";
}
```

### TypeStatistics, GetTypeStatistics

```cpp
struct TypeStatistics {
    const std::type_info* Type;   // the object's type, or the array type of a buffer (typeid(T[]))
    bool Buffers;                 // true for the buffers of the containers
    size_t ObjectSize;            // bytes of one object, or of one element of a buffer
    size_t LiveObjects;           // objects (or buffers) of this type after the cycle
    size_t LiveBytes;             // their bytes: the slots they occupy
    size_t Pages;                 // pages of this type's pools; 0 for buffers
};

static std::vector<TypeStatistics> GetTypeStatistics();
```

The live objects by type after a full cycle: what a heap that grows is made of. Objects are listed by their type; the buffers of the containers (`List`, `Array<T>`, the maps of `Deque`, the buckets of the hash tables) by their array type, `typeid(T[])` for elements `T`, with `Buffers == true`, the slot they occupy as their bytes and no pages, since the pages of buffers belong to size classes rather than to a type. `ObjectSize` is the slot size, at least `sizeof(T)`. Sorted by `LiveBytes`, descending, then by `LiveObjects`. Like `LiveObjects()`: a full cycle runs first, the caller's dead frames are zeroed, the caller waits for the cycle.

```cpp
for (auto& t : Collector::GetTypeStatistics()) {
    std::cout << (t.Buffers ? "buffers of " : "") << t.Type->name() << ": "
              << t.LiveObjects << " x " << t.ObjectSize << " B = " << t.LiveBytes << " B";
    if (!t.Buffers) {
        std::cout << ", " << t.Pages << " pages";
    }
    std::cout << '\n';
}
```

### CommittedMemory

```cpp
static size_t CommittedMemory() noexcept;
```

Bytes of managed memory committed right now: the part of the heap's reserved range backed by physical memory, in 2 MB chunks, free chunks kept for reuse included ([Memory](../../../garbage_collector/overview.md#memory)). The reservation itself (the process's virtual size) is not counted.

```cpp
std::cout << Collector::CommittedMemory() / 1048576 << " MB committed\n";
```

### MemoryLimit, SetMemoryLimit

```cpp
static size_t MemoryLimit() noexcept;
static void SetMemoryLimit(size_t bytes) noexcept;
```

The ceiling on committed managed memory, in bytes: by default 90% of the cgroup memory limit on Linux, or of the physical memory elsewhere (`config::HeapLimitPercent`). Above 75% of it (`config::HeapPressurePercent`) the collector cycles every 100 ms and returns every free chunk to the system at once. When an allocation would cross it, the allocation first forces a full collection and waits for it (not on a thread that is sweeping: a destructor run by the sweep gets the `std::bad_alloc` at once, since the cycle it would wait for is the one it is part of); if that does not free enough, it throws `std::bad_alloc` instead of letting the process run into the OOM killer. `SetMemoryLimit(0)` disables the ceiling. The setting takes effect for the next chunk committed; it does not shrink what is committed already.

```cpp
auto limit = Collector::MemoryLimit();                // the default ceiling
Collector::SetMemoryLimit(size_t(4) << 30);           // 4 GB
try {
    List<int> huge(size_t(2) << 30);                  // 8 GB of int: over the ceiling
} catch (const std::bad_alloc&) {
    // a full collection ran first; not enough was free
}
Collector::SetMemoryLimit(limit);                     // back to the default
```

### Referrer, GetReferrers, GetPathToRoot, Explain

```cpp
struct Referrer {
    enum class Kind { Object, Buffer, Stack, Cell, Unique, Weak };
    Kind From;
    const void* Holder;           // the object, buffer or block that holds the word; the word itself on a stack
    const std::type_info* Type;   // the holder's type; a buffer's element type (typeid(T[])); null for a stack word
    size_t Offset;                // the word's byte offset in the holder
    std::thread::id Thread;       // a stack word: the thread whose stack it is on
};

struct Retained {
    size_t Objects;
    size_t Bytes;
};

static std::tuple<PauseGuard, std::vector<Referrer>> GetReferrers(const void* p);
static std::tuple<PauseGuard, std::vector<Referrer>> GetPathToRoot(const void* p);
static Retained GetRetained(const void* p);
static void Explain(const void* p, std::ostream& out);
```

What holds an object. Both run a full cycle first and keep the collector paused while the `PauseGuard` lives, as `LiveObjects()` does, so that the live objects are exactly the marked ones and no page moves under the walk (the mutators run on; a word is read as the scan reads it). `p` may point into the object. One guard at a time: a call made while a guard lives waits for a cycle the paused collector cannot run.

`GetReferrers` lists every word that points at the object: the members of objects (`Object`, with the holder's type and the word's offset), the elements of buffers (`Buffer`, the element type as `typeid(T[])`, the offset from the buffer's start, header included), the cells of [`RootPtr`](RootPtr.md)s in unmanaged memory (`Cell`: the block and the cell's offset; the `RootPtr` that owns the cell is not known to the collector), the words of every thread's stack (`Stack`: the word's address and the thread's id; on the calling thread, the frames above the call, so a local that holds the object is listed), the object itself when a `UniquePtr` owns it (`Unique`), and the cells of `WeakPtr`s (`Weak`), which hold nothing.

`GetPathToRoot` is a chain from the object up to a root: `[0]` holds the object, `[1]` holds that holder, and so on to a root: an object a `UniquePtr` owns, a block of cells (a `Unique` link, a root by its state), a word on a stack. A search from the roots down, breadth first, the roots by state first, then the other threads' stacks, and the calling thread's frames above the call only when nothing else reaches the object: the caller holds the pointer it asks about and asks what else does, so a chain that ends on its own stack says that nothing else does. The search does not go through a weak cell: a `WeakPtr` holds nothing, so no chain leads through one (`GetReferrers` still lists it). Empty: `p` is not into a live managed object, or only the frames of the call hold it.

`GetRetained` is what dies with the object: the objects reachable from it and from nowhere else, itself included, and their bytes, the slots they occupy (a container's buffer at the slot of its size class). What is shared with another root stays out; a weak pointer holds nothing, so what is reachable only through a `WeakPtr` is not retained by its holder. A full cycle first and the collector paused for the walk, as above, the guard released on return; `{0, 0}` for a pointer that is not into a live managed object.

`Explain` writes the chain as text, one line per link, and what the object retains; or why there is no chain.

```cpp
struct Leaf { int value = 0; };
struct Node { Ptr<Node> next; Ptr<Leaf> leaf; };
UniquePtr head = Make<Node>();
head->next = Make<Node>();
head->next->leaf = Make<Leaf>();
Collector::Explain(head->next->leaf.Get(), std::cout);
// 0x100... is held by
//   a Node at 0x100..., the word at byte 0
//   a Node at 0x100..., the word at byte 8
//   a unique_ptr: the Node at 0x100... is its object
// and keeps alive 1 object, 4 bytes, itself included
```

### Stepper

```cpp
class Stepper {
public:
    enum class Phase { Start, Flipped, Registered, Roots, Marked, Swept, Released };
    explicit Stepper(bool full = true);
    ~Stepper();
    Phase Step() noexcept;                 // one gate: the phase the collector stands at
    Phase AdvanceTo(Phase p) noexcept;     // gates until the collector stands at the next `p`
    void FinishCycle() noexcept;           // to `Released`
    void Full(bool full) noexcept;         // the kind of the cycles from the next one on
    void Helpers(unsigned n) noexcept;     // this many helper threads for every pass, whatever the work; 0 the policy
    Phase Current() const noexcept;
};
```

The collector one gate at a time, for the tests of the engine. While a `Stepper` exists no cycle runs by itself: the collector stands at a gate, a boundary between the phases of a cycle, until `Step()` lets it through to the next, and the calling thread, the mutator of the test, does its work in between, in the window a race would otherwise have to land in by luck. The gates: `Start` (a cycle about to begin), `Flipped` (the epoch flipped, nothing registered yet: an object made now stays unregistered for this cycle), `Registered` (the pages, objects and threads of before the flip registered, the blocks of cells released), `Roots` (the stacks scanned, the dirty pages of a young cycle traced), `Marked` (the marking converged, the weak cells cleared), `Swept` (the garbage destroyed and freed), `Released` (the empty pages back in the heap: the cycle is over). The cycles are full unless the stepper is made with `full = false` or `Full(false)` is called. A cycle in flight when the stepper is made runs to its end unstepped; the destructor lets the collector run on by itself.

`Helpers(n)` forces `n` helper threads on every pass of the cycles from here on, however small the heap: the parallel marking with its work stealing, the sweep, the stack scan and the states pass, which the policy starts only from a million objects or 256 pages.

The stepper's thread must not wait for the collector between gates: `Collect`, `LiveObjectCount`, `LiveObjects` and `GetTypeStatistics` would deadlock. `GetStatistics()` reads the counters of the last completed cycle and is fine. The engine's own tests assert with it what the cycle's windows guarantee: an object made after the flip and released into an old object is neither swept this cycle nor lost by the next; one made before the flip and released after the roots were traced is reachable by the state of its release alone; a store into an old, marked object after its page was traced is found by the next young cycle through the card; a `WeakPtr` locked before the weak phase holds its object through the cycle and reads null after it; a pointer loaded from an `Atomic` after the stacks were scanned, its only other reference dropped, is a root by the state of the copy; a thread exiting before the scan takes its objects with it, one exiting after keeps them for the cycle; an object watched by an `ExpiryQueue` is kept for the drain; a block of cells is freed by the cycle after the one that saw its last cell go.

```cpp
struct Node { Ptr<Node> next; };
Collector::Stepper s(false);                     // young cycles
Ptr holder = Make<Node>();
s.FinishCycle();                                 // holder is old and marked
s.AdvanceTo(Collector::Stepper::Phase::Roots);   // the stacks scanned, the dirty pages traced
holder->next = Make<Node>();                     // stored into an old object after the trace: the card
s.FinishCycle();                                 // not swept: made after the flip
s.FinishCycle();                                 // registered now, found through the card
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

struct Node {
    Ptr<Node> next;
    int value;
};

// The stack is scanned conservatively: the pointers juggled here stay in a
// frame of their own, which the counts below zero before they count.
static void BuildAndDrop(size_t count) {
    Ptr<Node> head;
    for (size_t i : Range(count)) {
        Ptr node = Make<Node>();
        node->next = head;
        node->value = int(i);
        head = node;
    }
    std::cout << "with the list: " << Collector::LiveObjectCount() << " live objects\n";
}   // head is gone: the whole list is garbage

int main() {
    size_t before = Collector::LiveObjectCount();
    BuildAndDrop(1000);
    // the count runs a full cycle first and zeroes the frames BuildAndDrop left behind
    std::cout << "after the list: " << Collector::LiveObjectCount() - before << " new live objects\n";

    List<Ptr<Node>> kept;
    for (int i : Range(10)) {
        kept.Add(Make<Node>());
    }
    // what the live heap is made of, by type: the ten nodes and the list's buffer
    for (auto& t : Collector::GetTypeStatistics()) {
        if (*t.Type == typeid(Node) || *t.Type == typeid(Ptr<Node>[])) {
            std::cout << (t.Buffers ? "buffers of " : "") << t.Type->name() << ": "
                      << t.LiveObjects << " x " << t.ObjectSize << " B\n";
        }
    }

    Collector::Collect(true);        // optional, for the demonstration only: the collector runs its cycles by itself
    auto s = Collector::GetStatistics();
    std::cout << s.Cycles << " cycles, " << s.LiveObjects << " live objects, last cycle "
              << s.LastCycleMs << " ms, " << Collector::CommittedMemory() / 1048576
              << " MB committed of a " << Collector::MemoryLimit() / 1048576 << " MB ceiling\n";
    return 0;
}
```

The output of one run (the time of the cycle and the ceiling are the machine's):

```
with the list: 1000 live objects
after the list: 0 new live objects
4Node: 10 x 16 B
buffers of A_N4Sgcl3PtrI4NodeEE: 1 x 8 B
10 cycles, 11 live objects, last cycle 0.138625 ms, 2 MB committed of a 58982 MB ceiling
```

## See also

- [config](../../core/config.md): the constants behind the stack clearing, the memory ceiling, the generations and the helpers.
- Every struct and class here is the interface's own, PascalCase throughout; `Inner()` of a `PauseGuard` or a `Stepper` is the object underneath.
- [Ptr](Ptr.md), [UniquePtr](UniquePtr.md), [WeakPtr](WeakPtr.md), [ExpiryQueue](../Containers/ExpiryQueue.md).
- README: [Methods useful for state analysis](../../../garbage_collector/diagnostics.md#methods-useful-for-state-analysis), [Memory](../../../garbage_collector/overview.md#memory), [Generations](../../../garbage_collector/overview.md#generations), [Stack roots](../../../garbage_collector/overview.md#stack-roots), [Threads](../../async/README.md#threads), [The rules](../../core/README.md#the-rules).
