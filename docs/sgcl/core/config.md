# sgcl::config

```cpp
#include "sgcl/sgcl.h"        // or "sgcl/core/config.h"

namespace sgcl::config {
    // constants, all static constexpr
}
```

`sgcl/core/config.h` holds the constants that size and tune the collector: the page and chunk sizes of the managed heap, the memory ceiling, the stack clearing, when the helper threads join in, the generations. Every constant is `static constexpr` in `namespace sgcl::config`, readable from a program (`sgcl::config::page_size`); a few are macros first, `SGCL_...`, guarded by `#ifndef` so that a `-D` on the compiler's command line sets them without touching the header. The rest are changed by editing the header, which is a file of the header-only library in the program's include path.

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

### long_sleep_time, short_sleep_time, pressure_sleep_time

```cpp
static constexpr auto long_sleep_time = std::chrono::seconds(30);
static constexpr auto short_sleep_time = std::chrono::seconds(3);
static constexpr auto pressure_sleep_time = std::chrono::milliseconds(100);
```

How long the collector thread sleeps after a cycle when nothing wakes it. It is woken earlier by allocation: a thread that takes a fresh page when the pages allocated since the last cycle exceed a quarter of the pages live after it (plus 64), and by `force_collect()`. Otherwise it sleeps `long_sleep_time`, or `short_sleep_time` after a cycle whose sweep freed more than a quarter of what the previous cycle allocated (more garbage is likely coming), or `pressure_sleep_time` while the committed memory is above `heap_pressure_percent` of the ceiling. A program that allocates steadily never sees the long sleep.

### page_size, chunk_size, cache_line_size, l1_line_size

```cpp
static constexpr size_t page_size = 0x10000;        // 64 KB
static constexpr size_t chunk_size = 0x200000;      // 2 MB
static constexpr size_t cache_line_size = 128;
static constexpr size_t l1_line_size = 128;         // 64 but on Apple silicon
```

The managed heap is one virtual range reserved at first use and backed lazily; pages of `page_size` come from chunks of `chunk_size`, aligned to `chunk_size`, which is the unit of commit and decommit and huge-page friendly. Small objects live in pools of pages; buffers larger than a page take contiguous page ranges. A chunk is 32 pages (one 32-bit free mask), which the header asserts, as it asserts that `page_size` is a power of two of at most 64 KB. `cache_line_size` is the line on which structures written by different threads are kept apart: 128 bytes covers Apple silicon, x86 uses 64. `l1_line_size` is the line of the platform's L1 cache, the size of a block of cells a container's pointers share (16 on a 128-byte line, 8 on a 64-byte one). Read-only in practice: the sizes are part of the heap's layout and the defaults fit every platform the library runs on.

```cpp
static_assert(config::chunk_size == 32 * config::page_size);
size_t pages = collector::get_committed_memory() / config::page_size;
```

### heap_reserve_factor, heap_reserve_minimum, heap_reserve_floor

```cpp
static constexpr size_t heap_reserve_factor = 4;                     // times physical memory
static constexpr size_t heap_reserve_minimum = size_t(64) << 30;     // 64 GB
static constexpr size_t heap_reserve_floor = size_t(1) << 30;        // give up below 1 GB
```

The size of the virtual range reserved for the managed heap: `heap_reserve_factor` times the physical memory, at least `heap_reserve_minimum`; when the system refuses, half of it is tried, and so on down to `heap_reserve_floor`, below which the library prints a message and terminates the process. The reservation costs no physical memory; it shows up as the process's virtual size only ([Memory](../../garbage_collector/overview.md#memory)).

### SGCL_HEAP_FREE_CHUNK_RESERVE, heap_free_chunk_reserve

```cpp
#define SGCL_HEAP_FREE_CHUNK_RESERVE 32
static constexpr size_t heap_free_chunk_reserve = SGCL_HEAP_FREE_CHUNK_RESERVE;   // 32 chunks = 64 MB
```

Entirely free chunks are returned to the system once they have been free for a whole cycle, except this many kept committed for reuse, since recommitting costs a page fault per 4 KB. Under memory pressure nothing is kept. `get_committed_memory()` counts the kept chunks; a program that wants the committed size to follow its live size more closely lowers the reserve, at the cost of page faults when it grows again.

```sh
clang++ -std=c++20 -DSGCL_HEAP_FREE_CHUNK_RESERVE=8 app.cpp    # keep 16 MB, not 64
```

### heap_limit_percent, heap_pressure_percent

```cpp
static constexpr size_t heap_limit_percent = 90;
static constexpr size_t heap_pressure_percent = 75;
```

The default ceiling on committed managed memory is `heap_limit_percent` of the effective memory limit (the cgroup limit on Linux, else the physical memory), leaving the rest to everything outside the managed heap; `collector::set_memory_limit()` (`Collector::SetMemoryLimit()`) overrides it at run time, `0` disables it. Above `heap_pressure_percent` of the ceiling the collector cycles every `pressure_sleep_time` and returns every free chunk; at the ceiling an allocation forces a full collection and, failing that, throws `bad_alloc` ([collector](collector.md#get_memory_limit-set_memory_limit)).

```cpp
size_t ceiling = collector::get_memory_limit();          // heap_limit_percent of the machine's limit
size_t pressure = ceiling / 100 * config::heap_pressure_percent;
```

### stack_clear_size, stack_guard_margin

```cpp
static constexpr size_t stack_clear_size = 0x10000;    // 64 KB
static constexpr size_t stack_guard_margin = 0x8000;   // 32 KB
```

Stack roots are found by scanning the used part of every thread's stack, so words left behind by dead frames can keep an object alive until they are overwritten. `collector::force_collect()`, `get_live_object_count()`, `get_live_objects()` and `get_type_statistics()` (`Collector::Collect()`, `LiveObjectCount()`, `LiveObjects()`, `GetTypeStatistics()`) first zero `stack_clear_size` bytes of stack below the caller's frame (`collector::clear_stack(bytes)`, `Collector::ClearStack(bytes)`, any amount), never closer than `stack_guard_margin` to the end of the thread's stack, and never pages the stack has not touched yet ([Stack roots](../../garbage_collector/overview.md#stack-roots)).

```cpp
collector::clear_stack();                                 // stack_clear_size bytes
collector::clear_stack(4 * config::stack_clear_size); // 256 KB, for deep dead frames
```

### max_types_number

```cpp
static constexpr size_t max_types_number = 4096;
```

How many distinct managed types a program may create objects of (every `T` of `make_tracked<T>`, the cells of `weak_ptr` among them; the buffers of the containers and the frames of managed coroutines are allocated by size class, not by element type, and take a handful of slots in all): each gets an allocator slot per thread. The 4097th type prints a message to `stderr` and terminates the process; raise the constant in the header for a program that generates types.

### sweep_page_threshold, SGCL_SWEEP_THREADS_MAX, sweep_threads_max

```cpp
static constexpr size_t sweep_page_threshold = 256;   // 16 MB of pages
#define SGCL_SWEEP_THREADS_MAX 0
static constexpr size_t sweep_threads_max = SGCL_SWEEP_THREADS_MAX;
```

The sweep (destructors, freeing of slots) runs on the collector thread while it keeps up. Once a cycle has at least `sweep_page_threshold` pages of garbage to sweep, helper threads share the work: one per quarter of the threshold, up to `sweep_threads_max` of them (`0` = half the hardware threads, at least 1 and at most 8), parked between cycles, so they cost nothing while idle. `sweep_threads_max` also caps the helpers for marking and for reading the stacks. `collector::get_statistics()` reports how many were started and how many the last cycle used.

```sh
clang++ -std=c++20 -DSGCL_SWEEP_THREADS_MAX=2 app.cpp    # at most two helpers, for a program that needs its cores
```

### SGCL_MARK_OBJECT_THRESHOLD, mark_object_threshold

```cpp
#define SGCL_MARK_OBJECT_THRESHOLD (1024 * 1024)
static constexpr size_t mark_object_threshold = SGCL_MARK_OBJECT_THRESHOLD;
```

Marking shares the helpers as well, once the previous cycle marked at least `mark_object_threshold` objects: one helper per quarter of it, up to `sweep_threads_max`. Every thread traces from a stack of its own and hands half of it to an idle one; the mark bit is set atomically. Below the threshold the collector thread marks alone, which is cheaper for a small live set.

```sh
clang++ -std=c++20 -DSGCL_MARK_OBJECT_THRESHOLD=262144 app.cpp    # parallel marking from 256 K live objects
```

### SGCL_MARK_PREFETCH_WINDOW, mark_prefetch_window

```cpp
#define SGCL_MARK_PREFETCH_WINDOW 8
static constexpr unsigned mark_prefetch_window = SGCL_MARK_PREFETCH_WINDOW;
```

The parallel marking traces the objects it pops from its stack through a window of this many, each prefetched as it enters and traced as it leaves, so that the cache misses of a depth-first walk over a graph laid out by allocation overlap instead of waiting one at a time: a random graph marks in half the time; a tree laid out in the order it is traced gains nothing, the hardware prefetcher being ahead already. A power of two; `0` traces straight from the stack.

```sh
clang++ -std=c++20 -DSGCL_MARK_PREFETCH_WINDOW=16 app.cpp
```

### SGCL_BACKOFF_MAX, backoff_max

```cpp
#define SGCL_BACKOFF_MAX 4096    // arm64; 1024 on x86; 65536 without a pause instruction
static constexpr unsigned backoff_max = SGCL_BACKOFF_MAX;
```

The longest wait of the exponential backoff of a failed compare-exchange on a contended word ([concurrent::stack](../concurrent/stack.md)), in pause instructions: a thread that loses the exchange pauses once before its next attempt, twice as long after each next loss, up to this many. The cap is a time, some 40 µs, and the default count is that time over the cost of the platform's pause: `isb` on arm64 takes 9 ns on an Apple M2 (4096), `pause` on x86 about 40 ns on Skylake and later and a few on older cores (1024), and where there is no pause instruction a turn of the loop takes a nanosecond or less (65536). Sixteen threads at one word take the Treiber stack from 740 ns per operation to 23 with 4096 on arm64 and to 40 with 1024; what one operation may wait under that contention is the cap. `0` retries at once.

```sh
clang++ -std=c++20 -DSGCL_BACKOFF_MAX=1024 app.cpp
```

### SGCL_WORKERS, workers

```cpp
#define SGCL_WORKERS 0
static constexpr unsigned workers = SGCL_WORKERS;
```

The number of worker threads of the [scheduler](../async/scheduler.md); `0`, the default, is the hardware concurrency; 64 at most. The workers are started by the first `spawn`.

```sh
clang++ -std=c++20 -DSGCL_WORKERS=4 app.cpp
```

### SGCL_WORKER_SPIN_US, worker_spin_microseconds

```cpp
#define SGCL_WORKER_SPIN_US 20
static constexpr unsigned worker_spin_microseconds = SGCL_WORKER_SPIN_US;
```

How long a worker with nothing to run looks for work (the global queue, the other workers' queues) before it sleeps in the kernel, in microseconds: a task made ready in that window costs no wake through the kernel; a longer window costs a core per idle worker for that long.

### SGCL_BLOCKING_THREADS, blocking_threads

```cpp
#define SGCL_BLOCKING_THREADS 0
static constexpr unsigned blocking_threads = SGCL_BLOCKING_THREADS;
```

The most threads the [blocking pool](../async/blocking.md) grows to, for the calls a task hands off with `spawn_blocking`; `0`, the default, is the larger of 64 and four times the hardware concurrency: the threads block rather than compute, so there are more of them than cores. A job that finds every thread busy and the pool at its cap waits in the queue for the next thread to free up.

```sh
clang++ -std=c++20 -DSGCL_BLOCKING_THREADS=16 app.cpp
```

### SGCL_BLOCKING_IDLE_MS, blocking_idle_milliseconds

```cpp
#define SGCL_BLOCKING_IDLE_MS 10000
static constexpr unsigned blocking_idle_milliseconds = SGCL_BLOCKING_IDLE_MS;
```

How long an idle thread of the blocking pool waits for a job before it exits, in milliseconds: a program that stopped blocking has no threads for it after that long; `async::blocking_pool::set_idle_time` changes it at run time.

### io_buffer_size, io_copy_buffer_size

```cpp
static constexpr size_t io_buffer_size = 0x2000;        // 8 KB
static constexpr size_t io_copy_buffer_size = 0x8000;   // 32 KB
```

The buffers of io: the block a [buffered reader or writer](../io/buffered.md) holds in front of its stream, and the block `io::copy` moves the data through. Each is one managed array without a header, so a divisor of `page_size` fills whole pages: 8 KB gives eight to a page, 32 KB two.

### SGCL_HELPERS_GROWTH_THRESHOLD, helpers_growth_threshold

```cpp
#define SGCL_HELPERS_GROWTH_THRESHOLD (size_t(16) << 20)
static constexpr size_t helpers_growth_threshold = SGCL_HELPERS_GROWTH_THRESHOLD;   // 16 MB
```

Whether the helpers are used at all is decided by growth, not by allocation: a collector that keeps up leaves the live memory flat however much is allocated. When it grows by `helpers_growth_threshold` bytes between two cycle starts (measured from the lowest live memory seen since the helpers were last switched off, so that a few MB per cycle add up like one spike) the helpers switch on, and they stay on while the mutators keep allocating at least half the rate they allocated at when the growth was seen; below that they park. Reading the stacks is not subject to this policy: it does not come from allocation. `0` keeps the helpers always on (for measurements and tests). `statistics::helpers_enabled` says whether they are on for the next cycle.

### stack_scan_threshold, stack_scan_segment

```cpp
static constexpr size_t stack_scan_threshold = size_t(4) << 20;    // 4 MB
static constexpr size_t stack_scan_segment = size_t(256) << 10;    // 256 KB
```

The stacks are scanned by the collector thread while the pages they have used add up to less than `stack_scan_threshold` bytes; above it the helpers read the stacks too, in pieces of `stack_scan_segment` bytes, each collecting the words that point into the heap for the collector thread to mark (reading only, no shared state). Only the pages a stack has actually touched are read.

### SGCL_GENERATIONAL, generational, young_cycles_max, full_cycle_growth_percent

```cpp
#define SGCL_GENERATIONAL 1
static constexpr bool generational = SGCL_GENERATIONAL;
static constexpr unsigned young_cycles_max = 8;
static constexpr size_t full_cycle_growth_percent = 100;
```

Generational collection with sticky mark bits: a young cycle keeps the marks of the previous cycles, traces only the objects marked for the first time and the marked objects on pages a pointer was stored into since the previous cycle (the card the write barrier sets), and sweeps only the unmarked. Garbage among the marked objects waits for a full cycle: after `young_cycles_max` young cycles, when the live memory grew by `full_cycle_growth_percent` since the last full cycle, under memory pressure, and for every `force_collect()` and counting function, which therefore report complete collections ([Generations](../../garbage_collector/overview.md#generations)).

On by default: the young cycles cut the collector's CPU by a quarter to two thirds and the memory by a third on a large live heap, at 0.3 ns per store of a pointer into a heap object (the card: a shift and a byte read). `-DSGCL_GENERATIONAL=0` switches them off: every cycle is full and the barrier does no carding, for a program that links objects more than it allocates them and holds little. `statistics::full_cycles` then equals `statistics::cycles`.

```cpp
if constexpr (config::generational) {
    // full cycles are the exception: at most one per young_cycles_max young ones, unless forced
}
```

## Example

In `sgcl`:

```cpp
#include "sgcl/sgcl.h"
#include <algorithm>
#include <iostream>
#include <thread>

using namespace sgcl;

// Prints the configuration the program was built with, next to what the
// collector does with it. Build with -DSGCL_GENERATIONAL=0 or
// -DSGCL_SWEEP_THREADS_MAX=2 to see the values change.
int main() {
    std::cout << "page " << config::page_size / 1024 << " KB, chunk " << config::chunk_size / 1048576
              << " MB, " << config::heap_free_chunk_reserve << " free chunks kept committed\n";
    std::cout << "generational: " << (config::generational ? "yes" : "no") << ", a full cycle after "
              << config::young_cycles_max << " young ones or " << config::full_cycle_growth_percent << "% growth\n";
    size_t helpers = config::sweep_threads_max ? config::sweep_threads_max
                                             : std::min<size_t>(8, std::max<size_t>(1, std::thread::hardware_concurrency() / 2));
    std::cout << "helpers: at most " << helpers << ", sweeping from " << config::sweep_page_threshold << " pages, marking from "
              << config::mark_object_threshold << " objects, switched on by " << (config::helpers_growth_threshold >> 20)
              << " MB of growth\n";
    std::cout << "stack: " << config::stack_clear_size / 1024 << " KB zeroed before a count, "
              << config::stack_guard_margin / 1024 << " KB guard\n";

    // the ceiling the defaults produced on this machine, and the pressure line under it
    size_t ceiling = collector::get_memory_limit();
    std::cout << "ceiling " << (ceiling >> 20) << " MB (" << config::heap_limit_percent << "% of the limit), pressure above "
              << (ceiling / 100 * config::heap_pressure_percent >> 20) << " MB\n";

    // some work, then the counters that the constants above shape
    vector<tracked_ptr<int>> kept;
    for (int i : range(100000)) {
        kept.push_back(make_tracked<int>(i));
    }
    collector::force_collect(true);   // optional, for the demonstration only: the collector runs its cycles by itself
    auto s = collector::get_statistics();
    std::cout << s.cycles << " cycles, " << s.full_cycles << " full; helpers "
              << (s.helpers_enabled ? "on" : "off") << ", " << s.helper_threads << " started\n";
    return 0;
}
```

## See also

- [collector](collector.md): the functions that read and override what the constants set (`get_statistics`, `get_memory_limit`, `set_memory_limit`, `clear_stack`).
- README: [Generations](../../garbage_collector/overview.md#generations), [Memory](../../garbage_collector/overview.md#memory), [Stack roots](../../garbage_collector/overview.md#stack-roots), [Threads](../async/README.md#threads), [Dependencies and usage](../../../README.md#dependencies-and-usage).
- `tests/core/generational.cpp`, `tests/core/heap.cpp`, `tests/core/sweep.cpp`, `tests/core/marking.cpp`: the constants exercised.
