[sgcl](../../README.md) › [core](../README.md)

# sgcl::collector

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        struct statistics;
        struct type_statistics;
        struct referrer;
        struct retained;
        class stepper;
    };
}
```

`sgcl::collector` is the program's handle on the garbage collector: a class of static functions and the plain
structs they return, with no instance. The collector itself starts with the first managed object, on a thread of
its own, and runs its cycles by itself; nothing in a program has to call anything here. What the class offers is
for analysis and control: forcing a cycle and waiting for it, counting and listing the live objects, reading the
counters of the collector's work and the composition of the live heap by type, finding what holds an object, the
ceiling on managed memory, stepping the collector through a cycle in a test, and stopping the collector.

Four of the functions (`get_live_object_count`, `get_live_objects`, `get_type_statistics` and `force_collect`) run a
full collection first, so their answers are complete: a young cycle would leave garbage among the old objects
([Generations](../../../garbage_collector/overview.md#generations)). They also zero the unused stack below the caller's
frame first, because the stack is scanned conservatively and words left behind by dead frames would otherwise keep
objects alive and distort a count taken right after a scope
([Stack roots](../../../garbage_collector/overview.md#stack-roots)). What they cannot clear is the caller's own frame: a
raw pointer or an iterator kept there does retain its target, so a test that needs an exact count keeps its
pointer-juggling code in a helper function.

## Rules

- Every function may be called from any thread, at any time, concurrently with the mutators and with each other.
  None of them stops a mutator; `get_live_objects()`, `get_referrers()` and `get_path_to_root()` pause the
  collector while the `pause_guard` they return lives.
- `force_collect(true)`, `get_live_object_count()`, `get_live_objects()`, `get_type_statistics()`,
  `get_referrers()`, `get_path_to_root()`, `get_retained()` and `explain()` block the calling thread until a full
  cycle after the call has completed; they must not be called from a destructor of a managed object (a destructor
  runs on the collector's threads, inside the cycle they would wait for).
- `force_collect()` is optional in every program: the collector runs its cycles by itself. The examples and tests
  call it only to show or check a result at once.
- In the child of a `fork()` the collector does not run (the child has no thread but the one that forked): the
  child may read managed objects and `exec` or exit; `force_collect`, `terminate` and a managed allocation that
  needs a page terminate the child with a message ([Threads](../../async/README.md#threads)).
- The lists come back in `std::vector` and `std::tuple`, not in the library's containers, and that is deliberate:
  they are made while the collector is paused (and `get_live_objects` hands back the pause with them), when an
  allocation on the managed heap could wait for the very cycle the pause holds back, and the raw pointers of
  `get_live_objects` must not become objects the next cycle traces. They are the only std containers the library's
  interface returns.
- `get_live_object_count`, `get_live_objects`, `get_type_statistics`, `force_collect` and `clear_stack` are declared
  always-inline, so that no frame of their own lies between the caller and the area they zero.

### The memory limit

The collector keeps the committed managed memory under a ceiling: by default 90% of the cgroup memory limit on
Linux, or of the physical memory elsewhere (`config::heap_limit_percent`). Above 75% of it
(`config::heap_pressure_percent`) the collector cycles every 100 ms and returns every free chunk to the system at
once. When an allocation would cross it, the allocation first forces a full collection and waits for it; if that
does not free enough, the program prints one line to stderr and ends with `std::terminate()`, instead of running
into the OOM killer:

```text
sgcl: out of managed memory: N bytes committed, limit L
```

A destructor run by the sweep that allocates past the ceiling ends the program at once: the cycle it would wait for
is the one it is part of. Running out of managed memory is not an exception: no allocation of `make_tracked`, a
container, a string or a coroutine frame throws `bad_alloc`. With the ceiling disabled (`set_memory_limit(0)`), an
allocation the system refuses ends the program the same way, the line saying `no limit`.

Plain memory the library takes for itself outside the managed heap — the scratch arrays of [txt](../../txt/README.md),
a line and a batch of lines of [slog](../../slog/README.md), the bytes [io::read_all](../../io/read_all.md) gathers — and
an object of the system's that a call needs (the `CFData` of [heif](../../codec/heif/README.md)) end the program the same way
when the system refuses them, the line naming what was refused:

```text
sgcl: out of memory: a log line of N bytes was refused
```

What is taken through `operator new`, the program's or a `std` container's, stays the program's: its `operator new`
and new-handler decide.

The environment sets the default without a change to the program, as Go's `GOMEMLIMIT` does: `SGCL_MEMORY_LIMIT`
holds bytes (`536870912`, or with a suffix of powers of 1024: `512K`, `512M`, `2G`) or a percentage of the cgroup or
physical limit (`50%`). It is read once, when the heap is first used, and nowhere else; a call of
[set_memory_limit](set_memory_limit.md) or [set_memory_limit_percent](set_memory_limit_percent.md)
wins over it. A value that does not read (`12Q`, `0`, `150%`) is ignored with one line on stderr, and the 90%
taken. `SGCL_MEMORY_LIMIT=64M ./tool` runs the tool with a 64 MB ceiling.

### Its log

With `SGCL_LOG_PRINT_LEVEL` above 0 ([config](../config.md#sgcl_log_print_level)) the collector says what it does, in
lines on `std::cout`: level 1 its start and stop and every `force_collect`, level 2 a line per cycle, level 3 the
threads and its pauses. [slog::collector_log](../../slog/README.md#the-collectors-log) makes them records of a logger
instead (the second program under [Examples](#examples)).

Without the call the same lines go to `std::cout` as they always did (`benchmarks/heap/heap_parse.py` reads them
there). The collector's thread never touches managed memory for a line: it is copied into a queue of plain memory
and written by a thread that may log ([slog](../../slog/README.md#the-collectors-log)).

## Member types

| Type | Definition |
|---|---|
| `pause_guard` | a movable RAII object, a `std::unique_ptr` with a deleter of the library: while it lives, the collector is paused; its destruction, or `reset()`, resumes it |
| [statistics](../collector-statistics.md) | the counters of the collector's work, what `get_statistics` returns |
| [type_statistics](../collector-type_statistics.md) | the live objects of one type, an element of what `get_type_statistics` returns |
| [referrer](../collector-referrer.md) | a word that points at an object, an element of what `get_referrers` and `get_path_to_root` return |
| [retained](../collector-retained.md) | what dies with an object, what `get_retained` returns |
| [stepper](../collector-stepper/README.md) | the collector one gate at a time, for the tests of the engine |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `phase_names` | `{"registration", "states", "roots", "marking", "updated", "sweep", "release", "trim"}` | the names of the eight phases of a cycle, by the index of `statistics::phases_ms`; `static constexpr const char* [8]` |

## Member functions

#### Collection

| Function | Description |
|---|---|
| [force_collect](force_collect.md) | requests a full collection, and waits for it on request (static) |
| [clear_stack](clear_stack.md) | zeroes the unused stack below the caller's frame (static) |
| [terminate](terminate.md) | stops the collector (static) |

#### Statistics

| Function | Description |
|---|---|
| [get_statistics](get_statistics.md) | the counters of the collector's work, without waiting (static) |
| [get_type_statistics](get_type_statistics.md) | the live objects by type, after a full cycle (static) |
| [get_live_object_count](get_live_object_count.md) | the number of live objects, after a full cycle (static) |
| [get_live_objects](get_live_objects.md) | the addresses of the live objects, with the collector paused (static) |
| [get_committed_memory](get_committed_memory.md) | the managed memory committed now (static) |

#### Memory limit

| Function | Description |
|---|---|
| [get_memory_limit](get_memory_limit.md) | the ceiling on committed managed memory (static) |
| [set_memory_limit](set_memory_limit.md) | sets the ceiling in bytes (static) |
| [set_memory_limit_percent](set_memory_limit_percent.md) | sets the ceiling as a share of the memory the process may use (static) |

#### Referrers

| Function | Description |
|---|---|
| [get_referrers](get_referrers.md) | every word that points at an object (static) |
| [get_path_to_root](get_path_to_root.md) | a chain from an object up to a root (static) |
| [get_retained](get_retained.md) | what dies with an object (static) |
| [explain](explain.md) | the chain to a root and what the object retains, as text (static) |

## Examples

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    tracked_ptr<Node> next;
    int value;
};

// The stack is scanned conservatively: the pointers juggled here stay in a
// frame of their own, which the counts below zero before they count.
static void build_and_drop(size_t count) {
    tracked_ptr<Node> head;
    for (size_t i : range(count)) {
        tracked_ptr node = make_tracked<Node>();
        node->next = head;
        node->value = int(i);
        head = node;
    }
    println("with the list: {} live objects", collector::get_live_object_count());
}  // head is gone: the whole list is garbage

int main() {
    size_t before = collector::get_live_object_count();
    build_and_drop(1000);
    // the count runs a full cycle first and zeroes the frames build_and_drop left behind
    println("after the list: {} new live objects", collector::get_live_object_count() - before);

    vector<tracked_ptr<Node>> kept;
    for (int i : range(10)) {
        kept.push_back(make_tracked<Node>());
    }
    // what the live heap is made of, by type: the ten nodes and the vector's buffer
    for (auto& t : collector::get_type_statistics()) {
        if (*t.type == typeid(Node) || *t.type == typeid(tracked_ptr<Node>[])) {
            println("{}{}: {} x {} B", (t.buffers ? "buffers of " : ""), t.type->name(),
                    t.live_objects, t.object_size);
        }
    }

    // optional, for the demonstration only: the collector runs its cycles by itself
    collector::force_collect(true);
    auto s = collector::get_statistics();
    println("{} cycles, {} live objects, last cycle {} ms, {} MB committed of a {} MB ceiling",
            s.cycles, s.live_objects, s.last_cycle_ms, collector::get_committed_memory() / 1048576,
            collector::get_memory_limit() / 1048576);
}
```

Sample output:

```text
with the list: 1000 live objects
after the list: 3 new live objects
4Node: 10 x 16 B
buffers of A_N4sgcl11tracked_ptrI4NodeEE: 1 x 8 B
10 cycles, 14 live objects, last cycle 0.271959 ms, 2 MB committed of a 58982 MB ceiling
```

The collector's log as records of the default logger:

```cpp
#define SGCL_LOG_PRINT_LEVEL 2  // a line per cycle
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::collector_log();  // the lines from now on as records of the default logger
    collector::force_collect(true);  // optional, for the demonstration only
    slog::info("done");
}
```

Sample output:

```text
time=2026-09-29T11:02:15.207+02:00 level=INFO msg=collector verbosity=1 line="force collect and wait from id: 0x16b0f3000"
time=2026-09-29T11:02:15.208+02:00 level=INFO msg=collector verbosity=2 mem_allocs=3 mem_removed=0 total_mem=41 objects_created=1210 objects_removed=0 live_objects=1210 cycle=full helpers=false helpers_used=0 time_ms=0.412 total_time_ms=0.412
time=2026-09-29T11:02:15.209+02:00 level=INFO msg=done
```

## See also

- [config](../config.md): the constants behind the stack clearing, the memory ceiling, the generations and the helpers
- [tracked_ptr](../tracked_ptr/README.md), [unique_ptr](../unique_ptr/README.md), [weak_ptr](../weak_ptr/README.md), [expiry_queue](../expiry_queue/README.md)
- [Methods useful for state analysis](../../../garbage_collector/diagnostics.md#methods-useful-for-state-analysis),
  [Memory](../../../garbage_collector/overview.md#memory), [Generations](../../../garbage_collector/overview.md#generations),
  [Stack roots](../../../garbage_collector/overview.md#stack-roots): the collector and its diagnostics
- [Threads](../../async/README.md#threads), [README: The rules](../README.md#the-rules)
- `tests/core/statistics.cpp`, `tests/core/heap.cpp`: the counters and the memory ceiling exercised
