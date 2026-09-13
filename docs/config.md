# sgcl::config

```cpp
#include "sgcl/sgcl.h"        // or "sgcl/config.h"

namespace sgcl::config {
    // constants, all static constexpr
}
```

`sgcl/config.h` holds the constants that size and tune the collector: the page and chunk sizes of the managed heap, the memory ceiling, the stack clearing, when the helper threads join in, the generations. Every constant is `static constexpr` in `namespace sgcl::config`, readable from a program (`sgcl::config::PageSize`); a few are macros first, `SGCL_...`, guarded by `#ifndef` so that a `-D` on the compiler's command line sets them without touching the header. The rest are changed by editing the header, which is a file of the header-only library in the program's include path.

The library is header-only, so a constant is compiled into every translation unit that includes it: a `-D` (or an edit) must be the same for the whole program, or the translation units disagree about the layout of the heap.

## Setting a macro

On the command line, in CMake, or before the first include of a header of the library:

```sh
clang++ -std=c++20 -DSGCL_GENERATIONAL=0 -DSGCL_SWEEP_THREADS_MAX=4 ...
```

```cmake
target_compile_definitions(app PRIVATE SGCL_GENERATIONAL=0 SGCL_MARK_OBJECT_THRESHOLD=262144)
```

```cpp
#define SGCL_HEAP_FREE_CHUNK_RESERVE 8   // in every translation unit: use -D instead
#include "sgcl/sgcl.h"
```

## Members

### SGCL_LOG_PRINT_LEVEL

```cpp
#define SGCL_LOG_PRINT_LEVEL 0   // 0..3
```

What the library prints to `std::cout`, prefixed `[sgcl]`. `0` (the default) prints nothing. `1` prints the start, stop and termination of the collector thread, and every `force_collect()` and `get_live_objects()` call with the id of the calling thread. `2` adds one line per cycle: the pages allocated and freed since the previous cycle, the live pages, the objects created and removed, the live objects, the kind of the cycle (full or young), whether the helpers are on and how many the cycle used, the cycle's time and the total so far. `3` adds the registration and exit of every mutator thread and the collector's pauses for `get_live_objects()`. A diagnostic, not a log for production.

```sh
clang++ -std=c++20 -DSGCL_LOG_PRINT_LEVEL=2 app.cpp    # one line per cycle on stdout
```

### LongSleepTime, ShortSleepTime, PressureSleepTime

```cpp
static constexpr auto LongSleepTime = std::chrono::seconds(30);
static constexpr auto ShortSleepTime = std::chrono::seconds(3);
static constexpr auto PressureSleepTime = std::chrono::milliseconds(100);
```

How long the collector thread sleeps after a cycle when nothing wakes it. It is woken earlier by allocation: a thread that takes a fresh page when the pages allocated since the last cycle exceed a quarter of the pages live after it (plus 64), and by `force_collect()`. Otherwise it sleeps `LongSleepTime`, or `ShortSleepTime` after a cycle whose sweep freed more than a quarter of what the previous cycle allocated (more garbage is likely coming), or `PressureSleepTime` while the committed memory is above `HeapPressurePercent` of the ceiling. A program that allocates steadily never sees the long sleep.

### PageSize, ChunkSize, CacheLineSize

```cpp
static constexpr size_t PageSize = 0x10000;        // 64 KB
static constexpr size_t ChunkSize = 0x200000;      // 2 MB
static constexpr size_t CacheLineSize = 128;
```

The managed heap is one virtual range reserved at first use and backed lazily; pages of `PageSize` come from chunks of `ChunkSize`, aligned to `ChunkSize`, which is the unit of commit and decommit and huge-page friendly. Small objects live in pools of pages; buffers larger than a page take contiguous page ranges. A chunk is 32 pages (one 32-bit free mask), which the header asserts, as it asserts that `PageSize` is a power of two of at most 64 KB. `CacheLineSize` is the line on which structures written by different threads are kept apart: 128 bytes covers Apple silicon, x86 uses 64. Read-only in practice: the sizes are part of the heap's layout and the defaults fit every platform the library runs on.

```cpp
static_assert(sgcl::config::ChunkSize == 32 * sgcl::config::PageSize);
size_t pages = sgcl::collector::get_committed_memory() / sgcl::config::PageSize;
```

### HeapReserveFactor, HeapReserveMinimum, HeapReserveFloor

```cpp
static constexpr size_t HeapReserveFactor = 4;                     // times physical memory
static constexpr size_t HeapReserveMinimum = size_t(64) << 30;     // 64 GB
static constexpr size_t HeapReserveFloor = size_t(1) << 30;        // give up below 1 GB
```

The size of the virtual range reserved for the managed heap: `HeapReserveFactor` times the physical memory, at least `HeapReserveMinimum`; when the system refuses, half of it is tried, and so on down to `HeapReserveFloor`, below which the library prints a message and terminates the process. The reservation costs no physical memory; it shows up as the process's virtual size only ([Memory](../README.md#memory)).

### SGCL_HEAP_FREE_CHUNK_RESERVE, HeapFreeChunkReserve

```cpp
#define SGCL_HEAP_FREE_CHUNK_RESERVE 32
static constexpr size_t HeapFreeChunkReserve = SGCL_HEAP_FREE_CHUNK_RESERVE;   // 32 chunks = 64 MB
```

Entirely free chunks are returned to the system once they have been free for a whole cycle, except this many kept committed for reuse, since recommitting costs a page fault per 4 KB. Under memory pressure nothing is kept. `get_committed_memory()` counts the kept chunks; a program that wants the committed size to follow its live size more closely lowers the reserve, at the cost of page faults when it grows again.

```sh
clang++ -std=c++20 -DSGCL_HEAP_FREE_CHUNK_RESERVE=8 app.cpp    # keep 16 MB, not 64
```

### HeapLimitPercent, HeapPressurePercent

```cpp
static constexpr size_t HeapLimitPercent = 90;
static constexpr size_t HeapPressurePercent = 75;
```

The default ceiling on committed managed memory is `HeapLimitPercent` of the effective memory limit (the cgroup limit on Linux, else the physical memory), leaving the rest to everything outside the managed heap; `collector::set_memory_limit()` overrides it at run time, `0` disables it. Above `HeapPressurePercent` of the ceiling the collector cycles every `PressureSleepTime` and returns every free chunk; at the ceiling an allocation forces a full collection and, failing that, throws `std::bad_alloc` ([collector](collector.md#get_memory_limit-set_memory_limit)).

```cpp
size_t ceiling = sgcl::collector::get_memory_limit();          // HeapLimitPercent of the machine's limit
size_t pressure = ceiling / 100 * sgcl::config::HeapPressurePercent;
```

### StackClearSize, StackGuardMargin

```cpp
static constexpr size_t StackClearSize = 0x10000;    // 64 KB
static constexpr size_t StackGuardMargin = 0x8000;   // 32 KB
```

Stack roots are found by scanning the used part of every thread's stack, so words left behind by dead frames can keep an object alive until they are overwritten. `collector::force_collect()`, `get_live_object_count()`, `get_live_objects()` and `get_type_statistics()` first zero `StackClearSize` bytes of stack below the caller's frame (`collector::clear_stack(bytes)` any amount), never closer than `StackGuardMargin` to the end of the thread's stack, and never pages the stack has not touched yet ([Stack roots](../README.md#stack-roots)).

```cpp
sgcl::collector::clear_stack();                                 // StackClearSize bytes
sgcl::collector::clear_stack(4 * sgcl::config::StackClearSize); // 256 KB, for deep dead frames
```

### MaxTypesNumber

```cpp
static constexpr size_t MaxTypesNumber = 4096;
```

How many distinct managed types a program may create objects of (every `T` of `make_tracked<T>`, the cells of `weak_ptr` among them; the buffers of the containers and the frames of managed coroutines are allocated by size class, not by element type, and take a handful of slots in all): each gets an allocator slot per thread. The 4097th type prints a message to `stderr` and terminates the process; raise the constant in the header for a program that generates types.

### SweepPageThreshold, SGCL_SWEEP_THREADS_MAX, SweepThreadsMax

```cpp
static constexpr size_t SweepPageThreshold = 256;   // 16 MB of pages
#define SGCL_SWEEP_THREADS_MAX 0
static constexpr size_t SweepThreadsMax = SGCL_SWEEP_THREADS_MAX;
```

The sweep (destructors, freeing of slots) runs on the collector thread while it keeps up. Once a cycle has at least `SweepPageThreshold` pages of garbage to sweep, helper threads share the work: one per quarter of the threshold, up to `SweepThreadsMax` of them (`0` = half the hardware threads, at least 1 and at most 8), parked between cycles, so they cost nothing while idle. `SweepThreadsMax` also caps the helpers for marking and for reading the stacks. `collector::get_statistics()` reports how many were started and how many the last cycle used.

```sh
clang++ -std=c++20 -DSGCL_SWEEP_THREADS_MAX=2 app.cpp    # at most two helpers, for a program that needs its cores
```

### SGCL_MARK_OBJECT_THRESHOLD, MarkObjectThreshold

```cpp
#define SGCL_MARK_OBJECT_THRESHOLD (1024 * 1024)
static constexpr size_t MarkObjectThreshold = SGCL_MARK_OBJECT_THRESHOLD;
```

Marking shares the helpers as well, once the previous cycle marked at least `MarkObjectThreshold` objects: one helper per quarter of it, up to `SweepThreadsMax`. Every thread traces from a stack of its own and hands half of it to an idle one; the mark bit is set atomically. Below the threshold the collector thread marks alone, which is cheaper for a small live set.

```sh
clang++ -std=c++20 -DSGCL_MARK_OBJECT_THRESHOLD=262144 app.cpp    # parallel marking from 256 K live objects
```

### SGCL_HELPERS_GROWTH_THRESHOLD, HelpersGrowthThreshold

```cpp
#define SGCL_HELPERS_GROWTH_THRESHOLD (size_t(16) << 20)
static constexpr size_t HelpersGrowthThreshold = SGCL_HELPERS_GROWTH_THRESHOLD;   // 16 MB
```

Whether the helpers are used at all is decided by growth, not by allocation: a collector that keeps up leaves the live memory flat however much is allocated. When it grows by `HelpersGrowthThreshold` bytes between two cycle starts (measured from the lowest live memory seen since the helpers were last switched off, so that a few MB per cycle add up like one spike) the helpers switch on, and they stay on while the mutators keep allocating at least half the rate they allocated at when the growth was seen; below that they park. Reading the stacks is not subject to this policy: it does not come from allocation. `0` keeps the helpers always on (for measurements and tests). `statistics::helpers_enabled` says whether they are on for the next cycle.

### StackScanThreshold, StackScanSegment

```cpp
static constexpr size_t StackScanThreshold = size_t(4) << 20;    // 4 MB
static constexpr size_t StackScanSegment = size_t(256) << 10;    // 256 KB
```

The stacks are scanned by the collector thread while the pages they have used add up to less than `StackScanThreshold` bytes; above it the helpers read the stacks too, in pieces of `StackScanSegment` bytes, each collecting the words that point into the heap for the collector thread to mark (reading only, no shared state). Only the pages a stack has actually touched are read.

### SGCL_GENERATIONAL, Generational, YoungCyclesMax, FullCycleGrowthPercent

```cpp
#define SGCL_GENERATIONAL 1
static constexpr bool Generational = SGCL_GENERATIONAL;
static constexpr unsigned YoungCyclesMax = 8;
static constexpr size_t FullCycleGrowthPercent = 100;
```

Generational collection with sticky mark bits: a young cycle keeps the marks of the previous cycles, traces only the objects marked for the first time and the marked objects on pages a pointer was stored into since the previous cycle (the card the write barrier sets), and sweeps only the unmarked. Garbage among the marked objects waits for a full cycle: after `YoungCyclesMax` young cycles, when the live memory grew by `FullCycleGrowthPercent` since the last full cycle, under memory pressure, and for every `force_collect()` and counting function, which therefore report complete collections ([Generations](../README.md#generations)).

On by default: the young cycles cut the collector's CPU by a quarter to two thirds and the memory by a third on a large live heap, at 0.3 ns per store of a pointer into a heap object (the card: a shift and a byte read). `-DSGCL_GENERATIONAL=0` switches them off: every cycle is full and the barrier does no carding, for a program that links objects more than it allocates them and holds little. `statistics::full_cycles` then equals `statistics::cycles`.

```cpp
if constexpr (sgcl::config::Generational) {
    // full cycles are the exception: at most one per YoungCyclesMax young ones, unless forced
}
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <algorithm>
#include <iostream>
#include <thread>

// Prints the configuration the program was built with, next to what the
// collector does with it. Build with -DSGCL_GENERATIONAL=0 or
// -DSGCL_SWEEP_THREADS_MAX=2 to see the values change.
int main() {
    namespace config = sgcl::config;
    std::cout << "page " << config::PageSize / 1024 << " KB, chunk " << config::ChunkSize / 1048576
              << " MB, " << config::HeapFreeChunkReserve << " free chunks kept committed\n";
    std::cout << "generational: " << (config::Generational ? "yes" : "no") << ", a full cycle after "
              << config::YoungCyclesMax << " young ones or " << config::FullCycleGrowthPercent << "% growth\n";
    size_t helpers = config::SweepThreadsMax ? config::SweepThreadsMax
                                             : std::min<size_t>(8, std::max<size_t>(1, std::thread::hardware_concurrency() / 2));
    std::cout << "helpers: at most " << helpers << ", sweeping from " << config::SweepPageThreshold << " pages, marking from "
              << config::MarkObjectThreshold << " objects, switched on by " << (config::HelpersGrowthThreshold >> 20)
              << " MB of growth\n";
    std::cout << "stack: " << config::StackClearSize / 1024 << " KB zeroed before a count, "
              << config::StackGuardMargin / 1024 << " KB guard\n";

    // the ceiling the defaults produced on this machine, and the pressure line under it
    size_t ceiling = sgcl::collector::get_memory_limit();
    std::cout << "ceiling " << (ceiling >> 20) << " MB (" << config::HeapLimitPercent << "% of the limit), pressure above "
              << (ceiling / 100 * config::HeapPressurePercent >> 20) << " MB\n";

    // some work, then the counters that the constants above shape
    sgcl::vector<sgcl::tracked_ptr<int>> kept;
    for (int i = 0; i < 100000; ++i) {
        kept.push_back(sgcl::make_tracked<int>(i));
    }
    sgcl::collector::force_collect(true);   // optional, for the demonstration only: the collector runs its cycles by itself
    auto s = sgcl::collector::get_statistics();
    std::cout << s.cycles << " cycles, " << s.full_cycles << " full; helpers "
              << (s.helpers_enabled ? "on" : "off") << ", " << s.helper_threads << " started\n";
    return 0;
}
```

## See also

- [collector](collector.md): the functions that read and override what the constants set (`get_statistics`, `get_memory_limit`, `set_memory_limit`, `clear_stack`).
- README: [Generations](../README.md#generations), [Memory](../README.md#memory), [Stack roots](../README.md#stack-roots), [Threads](../README.md#threads), [Dependencies and usage](../README.md#dependencies-and-usage).
- `tests/generational.cpp`, `tests/heap.cpp`, `tests/sweep.cpp`, `tests/marking.cpp`: the constants exercised.
