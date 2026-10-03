[sgcl](../README.md) › [core](README.md)

# sgcl::config

```cpp
#include "sgcl/core/config.h"   // or "sgcl/core.h"

#define SGCL_LOG_PRINT_LEVEL 0

namespace sgcl::config {
    static constexpr auto long_sleep_time = std::chrono::seconds(30);
    static constexpr auto short_sleep_time = std::chrono::seconds(3);
    static constexpr auto pressure_sleep_time = std::chrono::milliseconds(100);
    static constexpr size_t page_size = 0x10000;
    static constexpr size_t chunk_size = 0x200000;
    static constexpr size_t cache_line_size = 128;
    static constexpr size_t l1_line_size = 128;   // on Apple silicon; 64 elsewhere
    static constexpr size_t heap_reserve_factor = 4;
    static constexpr size_t heap_reserve_minimum = size_t(64) << 30;
    static constexpr size_t heap_reserve_floor = size_t(1) << 30;
    static constexpr size_t heap_free_chunk_reserve = SGCL_HEAP_FREE_CHUNK_RESERVE;
    static constexpr size_t heap_limit_percent = 90;
    static constexpr size_t heap_pressure_percent = 75;
    static constexpr size_t stack_clear_size = 0x10000;
    static constexpr size_t stack_guard_margin = 0x8000;
    static constexpr size_t max_types_number = 4096;
    static constexpr size_t sweep_page_threshold = 256;
    static constexpr size_t sweep_threads_max = SGCL_SWEEP_THREADS_MAX;
    static constexpr size_t mark_object_threshold = SGCL_MARK_OBJECT_THRESHOLD;
    static constexpr unsigned mark_prefetch_window = SGCL_MARK_PREFETCH_WINDOW;
    static constexpr unsigned backoff_max = SGCL_BACKOFF_MAX;
    static constexpr unsigned workers = SGCL_WORKERS;
    static constexpr unsigned worker_spin_microseconds = SGCL_WORKER_SPIN_US;
    static constexpr unsigned blocking_threads = SGCL_BLOCKING_THREADS;
    static constexpr unsigned blocking_idle_milliseconds = SGCL_BLOCKING_IDLE_MS;
    static constexpr size_t io_buffer_size = 0x2000;
    static constexpr size_t io_copy_buffer_size = 0x8000;
    static constexpr size_t helpers_growth_threshold = SGCL_HELPERS_GROWTH_THRESHOLD;
    static constexpr size_t stack_scan_threshold = size_t(4) << 20;
    static constexpr size_t stack_scan_segment = size_t(256) << 10;
    static constexpr bool generational = SGCL_GENERATIONAL;
    static constexpr unsigned young_cycles_max = 8;
    static constexpr size_t full_cycle_growth_percent = 100;
}
```

`sgcl/core/config.h` holds the constants that size and tune the collector: the page and chunk sizes of the managed
heap, the memory ceiling, the stack clearing, when the helper threads join in, the generations; and the defaults of
the scheduler, the blocking pool and the buffers of io. Every constant is `static constexpr` in
`namespace sgcl::config`, readable from a program (`sgcl::config::page_size`); a few are macros first, `SGCL_...`,
guarded by `#ifndef` so that a `-D` on the compiler's command line sets them without touching the header. The rest
are changed by editing the header, which is a file of the header-only library in the program's include path.

## Rules

- The library is header-only, so a constant is compiled into every translation unit that includes it: a `-D` (or
  an edit) must be the same for the whole program, or the translation units disagree about the layout of the heap.
- The sizes of the heap (`page_size`, `chunk_size`, the lines) are part of its layout, and the defaults fit every
  platform the library runs on: they are read-only in practice. The header asserts that `page_size` is a power of
  two of at most 64 KB and that a chunk is 32 pages.

### Setting a macro

On the command line or in CMake:

```sh
clang++ -std=c++20 -DSGCL_GENERATIONAL=0 -DSGCL_SWEEP_THREADS_MAX=4 ...
```

```cmake
target_compile_definitions(app PRIVATE SGCL_GENERATIONAL=0 SGCL_MARK_OBJECT_THRESHOLD=262144)
```

A `#define SGCL_HEAP_FREE_CHUNK_RESERVE 8` before the first include of a header of the library does the same, but it
must then stand in every translation unit: `-D` is the way.

### SGCL_LOG_PRINT_LEVEL

What the library prints to `std::cout`, prefixed `[sgcl]`, from `0` to `3`. `0` (the default) prints nothing. `1`
prints the start, stop and termination of the collector thread, and every `force_collect()` and
`get_live_objects()` call with the id of the calling thread. `2` adds one line per cycle: the pages allocated and
freed since the previous cycle, the live pages, the objects created and removed, the live objects, the kind of the
cycle (full or young), whether the helpers are on and how many the cycle used, the cycle's time and the total so
far. `3` adds the registration and exit of every mutator thread and the collector's pauses for
`get_live_objects()`. A diagnostic, not a log for production. [slog::collector_log](../slog/README.md#the-collectors-log)
makes the same lines records of a logger instead ([collector: its log](collector/README.md#its-log)).
`clang++ -std=c++20 -DSGCL_LOG_PRINT_LEVEL=2 app.cpp` prints one line per cycle on stdout.

### Release builds and symbols

The library is header-only, so every inline function and template a program instantiates from it is, by default, a
weak exported symbol of the program: its mangled name goes into the string table, and the dynamic linker binds it
at start. In a program of any size this is more than the code. The async example (a generator, a task, `println`)
built with `-O3` is 1.43 MB, of which 621 KB is code and 645 KB the linker's tables: 347 KB of symbol names, 133 KB
of bindings, 62 KB of the export list. Hidden visibility and no local symbol table bring the same program to 788 KB,
with the code unchanged.

The CMake option `SGCL_HIDDEN_VISIBILITY`, on by default, does that for every program that links the `sgcl` target,
in a Release build only: it adds `-fvisibility=hidden -fvisibility-inlines-hidden` to the compilation and `-Wl,-x`
to the link. It has no effect with MSVC, whose symbols are hidden unless exported.

The flags apply to the whole program that links `sgcl`, not to the library alone. Turn the option off in two cases:

- **A shared library (`.dylib`, `.so`) that links `sgcl` and has an API of its own:** with hidden visibility, its own
  symbols are not exported either.
- **A process in which more than one image uses sgcl** (the program and a library loaded into it): the collector,
  the heap and the type tables live in inline functions, which the dynamic linker merges into one copy only while
  they are exported. Hidden, each image has a collector and a heap of its own, and an object made in one is not seen
  by the other's collector.

```cmake
set(SGCL_HIDDEN_VISIBILITY OFF CACHE BOOL "" FORCE)   # before add_subdirectory(sgcl)
add_subdirectory(sgcl)
target_link_libraries(app PRIVATE sgcl)
```

Without CMake, the same flags on the command line of a Release build:

```sh
clang++ -std=c++20 -O3 -DNDEBUG -fvisibility=hidden -fvisibility-inlines-hidden -Wl,-x app.cpp -o app
```

## Member objects

The constants, each with the macro that sets it where there is one. A macro is set with `-D`
([Setting a macro](#setting-a-macro)); the other constants by editing the header.

#### The collector's sleep

| Constant | Value | Description |
|---|---|---|
| `long_sleep_time` | `std::chrono::seconds(30)` | How long the collector thread sleeps after a cycle when nothing wakes it. It is woken earlier by allocation, when a thread takes a fresh page and the pages allocated since the last cycle exceed a quarter of the pages live after it (plus 64), and by `force_collect()`. A program that allocates steadily never sees the long sleep. |
| `short_sleep_time` | `std::chrono::seconds(3)` | The sleep after a cycle whose sweep freed more than a quarter of what the previous cycle allocated: more garbage is likely coming. |
| `pressure_sleep_time` | `std::chrono::milliseconds(100)` | The sleep while the committed memory is above `heap_pressure_percent` of the ceiling. |

#### The managed heap

| Constant | Value | Description |
|---|---|---|
| `page_size` | `0x10000`, 64 KB | The page of the managed heap. Small objects live in pools of pages; buffers larger than a page take contiguous page ranges. `collector::get_committed_memory() / config::page_size` is the number of pages committed. |
| `chunk_size` | `0x200000`, 2 MB | The heap is one virtual range reserved at first use and backed lazily; pages come from chunks of this size, aligned to it, which are the unit of commit and decommit and huge-page friendly. A chunk is 32 pages: one 32-bit free mask. |
| `cache_line_size` | `128` | The line on which structures written by different threads are kept apart: 128 bytes covers Apple silicon, x86 uses 64. |
| `l1_line_size` | `128` on Apple silicon, `64` elsewhere | The line of the platform's L1 cache: the size of a block of cells a container's pointers share, 16 cells on a 128-byte line, 8 on a 64-byte one. |
| `heap_reserve_factor` | `4` | The virtual range reserved for the managed heap is this many times the physical memory, at least `heap_reserve_minimum`. The reservation costs no physical memory; it shows up as the process's virtual size only ([Memory](../../garbage_collector/overview.md#memory)). |
| `heap_reserve_minimum` | `size_t(64) << 30`, 64 GB | The smallest range reserved. |
| `heap_reserve_floor` | `size_t(1) << 30`, 1 GB | When the system refuses the reservation, half of it is tried, and so on down to this, below which the library prints a message and terminates the process. |
| `heap_free_chunk_reserve` (`SGCL_HEAP_FREE_CHUNK_RESERVE`) | `32`, 64 MB | Entirely free chunks are returned to the system once they have been free for a whole cycle, except this many, kept committed for reuse, since recommitting costs a page fault per 4 KB. Under memory pressure nothing is kept. `collector::get_committed_memory()` counts the kept chunks; a program that wants the committed size to follow its live size more closely lowers the reserve, at the cost of page faults when it grows again: `-DSGCL_HEAP_FREE_CHUNK_RESERVE=8` keeps 16 MB, not 64. |
| `heap_limit_percent` | `90` | The default ceiling on committed managed memory, a share of the effective memory limit (the cgroup limit on Linux, else the physical memory), leaving the rest to everything outside the managed heap. `SGCL_MEMORY_LIMIT` in the environment (bytes with K, M or G, or a percentage, read once when the heap is first used) and `collector::set_memory_limit()` override it at run time, the call over the environment; `0` disables it. At the ceiling an allocation forces a full collection and, failing that, ends the program with a line on stderr ([collector](collector/README.md#the-memory-limit)). |
| `heap_pressure_percent` | `75` | Above this share of the ceiling (`collector::get_memory_limit() / 100 * config::heap_pressure_percent` bytes) the collector cycles every `pressure_sleep_time` and returns every free chunk at once. |
| `max_types_number` | `4096` | How many distinct managed types a program may create objects of: every `T` of `make_tracked<T>`, the cells of `weak_ptr` among them; the buffers of the containers and the frames of managed coroutines are allocated by size class, not by element type, and take a handful of slots in all. Each type gets an allocator slot per thread. The 4097th type prints a message to `stderr` and terminates the process; a program that generates types raises the constant in the header. |

#### The stacks

| Constant | Value | Description |
|---|---|---|
| `stack_clear_size` | `0x10000`, 64 KB | Stack roots are found by scanning the used part of every thread's stack, so words left behind by dead frames can keep an object alive until they are overwritten. `collector::force_collect()`, `get_live_object_count()`, `get_live_objects()` and `get_type_statistics()` first zero this many bytes of stack below the caller's frame; `collector::clear_stack(bytes)` zeroes any amount ([Stack roots](../../garbage_collector/overview.md#stack-roots)). |
| `stack_guard_margin` | `0x8000`, 32 KB | The zeroing never comes closer than this to the end of the thread's stack, and never touches pages the stack has not touched yet. |
| `stack_scan_threshold` | `size_t(4) << 20`, 4 MB | The stacks are scanned by the collector thread while the pages they have used add up to less than this; above it the helpers read the stacks too. Only the pages a stack has actually touched are read. |
| `stack_scan_segment` | `size_t(256) << 10`, 256 KB | The piece of a stack a helper reads at a time, collecting the words that point into the heap for the collector thread to mark: reading only, no shared state. |

#### The helper threads

| Constant | Value | Description |
|---|---|---|
| `sweep_page_threshold` | `256`, 16 MB of pages | The sweep (destructors, freeing of slots) runs on the collector thread while it keeps up. Once a cycle has at least this many pages of garbage to sweep, helper threads share the work: one per quarter of the threshold, up to `sweep_threads_max`, parked between cycles, so they cost nothing while idle. |
| `sweep_threads_max` (`SGCL_SWEEP_THREADS_MAX`) | `0` | The most helper threads, for the sweep, the marking and the reading of the stacks; `0` is half the hardware threads, at least 1 and at most 8. `collector::get_statistics()` reports how many were started and how many the last cycle used. `-DSGCL_SWEEP_THREADS_MAX=2`: at most two helpers, for a program that needs its cores. |
| `mark_object_threshold` (`SGCL_MARK_OBJECT_THRESHOLD`) | `1024 * 1024` | Marking shares the helpers once the previous cycle marked at least this many objects: one helper per quarter of it, up to `sweep_threads_max`. Every thread traces from a stack of its own and hands half of it to an idle one; the mark bit is set atomically. Below the threshold the collector thread marks alone, which is cheaper for a small live set. `-DSGCL_MARK_OBJECT_THRESHOLD=262144`: parallel marking from 256 K live objects. |
| `mark_prefetch_window` (`SGCL_MARK_PREFETCH_WINDOW`) | `8` | The parallel marking traces the objects it pops from its stack through a window of this many, each prefetched as it enters and traced as it leaves, so that the cache misses of a depth-first walk over a graph laid out by allocation overlap instead of waiting one at a time: a random graph marks in half the time (5.3 ns per object instead of 9.5 for 65536 roots); a tree laid out in the order it is traced gains nothing, the hardware prefetcher being ahead already. A power of two; `0` traces straight from the stack. |
| `helpers_growth_threshold` (`SGCL_HELPERS_GROWTH_THRESHOLD`) | `size_t(16) << 20`, 16 MB | Whether the helpers are used at all is decided by growth, not by allocation: a collector that keeps up leaves the live memory flat however much is allocated. When the live memory grows by this many bytes between two cycle starts, measured from the lowest live memory seen since the helpers were last switched off (so that a few MB per cycle add up like one spike), the helpers switch on, and they stay on while the mutators keep allocating at least half the rate they allocated at when the growth was seen; below that they park. Reading the stacks is not subject to this policy: it does not come from allocation. `0` keeps the helpers always on, for measurements and tests. `statistics::helpers_enabled` says whether they are on for the next cycle. |

#### The generations

| Constant | Value | Description |
|---|---|---|
| `generational` (`SGCL_GENERATIONAL`) | `1`, `true` | Generational collection with sticky mark bits: a young cycle keeps the marks of the previous cycles, traces only the objects marked for the first time and the marked objects on pages a pointer was stored into since the previous cycle (the card the write barrier sets), and sweeps only the unmarked. Garbage among the marked objects waits for a full cycle ([Generations](../../garbage_collector/overview.md#generations)). On by default: the young cycles cut the collector's CPU by a quarter to two thirds and the memory by a third on a large live heap, at 0.3 ns per store of a pointer into a heap object (the card: a shift and a byte read). `-DSGCL_GENERATIONAL=0` switches them off: every cycle is full and the barrier does no carding, for a program that links objects more than it allocates them and holds little; `statistics::full_cycles` then equals `statistics::cycles`. |
| `young_cycles_max` | `8` | A full cycle runs after this many young ones; also when the live memory grew by `full_cycle_growth_percent` since the last full cycle, under memory pressure, and for every `force_collect()` and counting function, which therefore report complete collections. |
| `full_cycle_growth_percent` | `100` | The growth of the live memory since the last full cycle that makes the next one full. |

#### Concurrency

| Constant | Value | Description |
|---|---|---|
| `backoff_max` (`SGCL_BACKOFF_MAX`) | `4096` on arm64, `1024` on x86, `65536` elsewhere | The longest wait of the exponential backoff of a failed compare-exchange on a contended word ([concurrent::stack](../concurrent/stack/README.md)), in pause instructions: a thread that loses the exchange pauses once before its next attempt, twice as long after each next loss, up to this many. The cap is a time, some 40 µs, and the default count is that time over the cost of the platform's pause: `isb` on arm64 takes 9 ns on an Apple M2 (4096), `pause` on x86 about 40 ns on Skylake and later and a few on older cores (1024), and where there is no pause instruction a turn of the loop takes a nanosecond or less (65536). Sixteen threads at one word take the Treiber stack from 740 ns per operation to 23 with 4096 on arm64 and to 40 with 1024; what one operation may wait under that contention is the cap. `0` retries at once. |
| `workers` (`SGCL_WORKERS`) | `0` | The number of worker threads of the [scheduler](../async/scheduler/README.md) when neither the program (`async::scheduler::set_workers`) nor the environment (`SGCL_WORKERS=4 ./app`, read once at the scheduler's first start) says otherwise; `0` is the hardware concurrency; 64 at most. The workers are started by the first `spawn`. |
| `worker_spin_microseconds` (`SGCL_WORKER_SPIN_US`) | `20` | How long a worker with nothing to run looks for work (the global queue, the other workers' queues) before it sleeps in the kernel, in microseconds: a task made ready in that window costs no wake through the kernel; a longer window costs a core per idle worker for that long. The default when neither `async::scheduler::set_worker_spin` nor `SGCL_WORKER_SPIN_US` in the environment sets it. |
| `blocking_threads` (`SGCL_BLOCKING_THREADS`) | `0` | The most threads the [blocking pool](../async/spawn_blocking.md) grows to, for the calls a task hands off with `spawn_blocking`, when neither `async::blocking_pool::set_threads` nor `SGCL_BLOCKING_THREADS` in the environment sets it; `0` is the larger of 64 and four times the hardware concurrency: the threads block rather than compute, so there are more of them than cores. A job that finds every thread busy and the pool at its cap waits in the queue for the next thread to free up. |
| `blocking_idle_milliseconds` (`SGCL_BLOCKING_IDLE_MS`) | `10000` | How long an idle thread of the blocking pool waits for a job before it exits, in milliseconds: a program that stopped blocking has no threads for it after that long; `async::blocking_pool::set_idle_time` changes it at run time. |

#### The buffers of io

| Constant | Value | Description |
|---|---|---|
| `io_buffer_size` | `0x2000`, 8 KB | The block a [buffered reader or writer](../io/buffered_reader/README.md) holds in front of its stream. A buffered reader's is one managed array without a header, since the lines it hands out are slices of it, so a divisor of `page_size` fills whole pages: 8 KB gives eight to a page. A buffered writer's is unmanaged memory of its own until its first async operation, and managed from its first async write on. |
| `io_copy_buffer_size` | `0x8000`, 32 KB | The block `io::copy` moves the data through, on the stack of the call. `async_copy`'s block is managed, since the operation may run on the blocking pool and outlive the frame of a task let go of: the slice it is given holds the block. |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>

using namespace sgcl;

// Prints the configuration the program was built with, next to what the
// collector does with it. Build with -DSGCL_GENERATIONAL=0 or
// -DSGCL_SWEEP_THREADS_MAX=2 to see the values change.
int main() {
    println("page {} KB, chunk {} MB, {} free chunks kept committed", config::page_size / 1024,
            config::chunk_size / 1048576, config::heap_free_chunk_reserve);
    println("generational: {}, a full cycle after {} young ones or {}% growth",
            (config::generational ? "yes" : "no"), config::young_cycles_max,
            config::full_cycle_growth_percent);
    size_t helpers = config::sweep_threads_max
        ? config::sweep_threads_max
        : std::min<size_t>(8, std::max<size_t>(1, thread::hardware_concurrency() / 2));
    println("helpers: at most {}, sweeping from {} pages, marking from {} objects, "
            "switched on by {} MB of growth", helpers, config::sweep_page_threshold,
            config::mark_object_threshold, (config::helpers_growth_threshold >> 20));
    println("stack: {} KB zeroed before a count, {} KB guard", config::stack_clear_size / 1024,
            config::stack_guard_margin / 1024);

    // the ceiling the defaults produced on this machine, and the pressure line under it
    size_t ceiling = collector::get_memory_limit();
    println("ceiling {} MB ({}% of the limit), pressure above {} MB", (ceiling >> 20),
            config::heap_limit_percent, (ceiling / 100 * config::heap_pressure_percent >> 20));

    // some work, then the counters that the constants above shape
    vector<tracked_ptr<int>> kept;
    for (int i : range(100000)) {
        kept.push_back(make_tracked<int>(i));
    }
    // optional, for the demonstration only: the collector runs its cycles by itself
    collector::force_collect(true);
    auto s = collector::get_statistics();
    println("{} cycles, {} full; helpers {}, {} started; {} pages committed", s.cycles,
            s.full_cycles, (s.helpers_enabled ? "on" : "off"), s.helper_threads,
            collector::get_committed_memory() / config::page_size);
}
```

Sample output:

```text
page 64 KB, chunk 2 MB, 32 free chunks kept committed
generational: yes, a full cycle after 8 young ones or 100% growth
helpers: at most 8, sweeping from 256 pages, marking from 1048576 objects, switched on by 16 MB of growth
stack: 64 KB zeroed before a count, 32 KB guard
ceiling 58982 MB (90% of the limit), pressure above 44236 MB
4 cycles, 2 full; helpers off, 0 started; 64 pages committed
```

## See also

- [collector](collector/README.md): the functions that read and override what the constants set (`get_statistics`,
  `get_memory_limit`, `set_memory_limit`, `clear_stack`)
- [Generations](../../garbage_collector/overview.md#generations), [Memory](../../garbage_collector/overview.md#memory),
  [Stack roots](../../garbage_collector/overview.md#stack-roots): how the collector uses them
- [Threads](../async/README.md#threads), [Dependencies and usage](../../../README.md#dependencies-and-usage)
- `tests/core/generational.cpp`, `tests/core/heap.cpp`, `tests/core/sweep.cpp`, `tests/core/marking.cpp`: the
  constants exercised
