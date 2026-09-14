# Diagnostics

What the library gives a program to find out what the collector is doing, what is alive and why, and where a rule was broken; and how to use it, case by case. Everything here is in [`collector`](collector.md), [`config`](config.md) and the debug assertions of the pointers; this page is the map.

## The tools

| tool | what it tells | cost |
|---|---|---|
| `collector::get_statistics()` | the cycles so far, the last cycle's wall time and its phases, the helper threads, the live objects the last cycle marked, the managed bytes in use and committed | a few atomic reads, never waits |
| `collector::get_live_object_count()` | the objects alive after a full cycle, run for the call | a full cycle, the caller waits |
| `collector::get_type_statistics()` | the live objects and bytes by type, buffers by their element type, pages by type; sorted by bytes | a full cycle, the caller waits |
| `collector::get_live_objects()` | the addresses of every live object, with the collector paused while the `pause_guard` lives | a full cycle, the collector paused |
| `collector::get_referrers(p)` | every word that points at the object: members, buffer elements, cells, stack words, a `unique_ptr`, weak cells | a full cycle, the collector paused, a pass over the heap and the stacks |
| `collector::get_path_to_root(p)`, `explain(p, out)` | a chain from the object up to a root, or the reason there is none; `explain` adds what the object retains | a full cycle, the collector paused, a search from the roots |
| `collector::get_retained(p)` | what dies with the object: the objects reachable from it and from nowhere else, and their bytes | a full cycle, the collector paused, two passes over the heap |
| `collector::clear_stack()` | zeroes the dead frames below the caller: what a conservative scan would otherwise still find | a `memset` of the stack below |
| `collector::stepper` | the collector one gate at a time, for the tests of the engine | the cycle held by the test |
| `tracked_ptr::type()`, `is<U>()`, `as<U>()` | the dynamic type of an object without virtual functions | a page lookup |
| `lldb/sgcl.py` | the debugger's view: a pointer with its address, mode and slot state, its object as a child; a container with its size and elements | `command script import` once |
| debug builds (no `-DNDEBUG`) | the rules asserted where they are broken: a `tracked_ptr` in unmanaged memory, an alias into a buffer, a `weak_ptr` to an object a `unique_ptr` owns | the assertions |
| `-DSGCL_LOG_PRINT_LEVEL=n` | the collector's log on `std::cout`: threads, forced collections, one line per cycle, the pauses | a line per event |
| `-DSGCL_TRACE_STACK` | every marking decision on `std::cerr`: which stack word retained what, which state was still reachable, which page was dirty | a line per object per cycle: for a small program |
| `-DSGCL_SANITIZER=address|thread` (CMake) | the tests and examples under a sanitizer; the library's own reads of other threads' memory are annotated | a sanitizer build |
| a message and `std::terminate` | the library's own failures: no address space for the heap, no stack range for a thread, too many types, a pointer where the map said data, the child of a fork | |

## Cases

### The memory grows: what is it made of

Start from the types, without stopping anything for long:

```cpp
for (auto& t : gc::collector::get_type_statistics()) {          // a full cycle first: what is alive, not what waits for one
    std::cout << t.type->name() << (t.buffers ? " buffers" : "") << ": "
              << t.live_objects << " objects, " << t.live_bytes << " bytes, " << t.pages << " pages\n";
}
```

The list is sorted by bytes, so the first lines are the answer in most cases: a type that should be a few hundred objects and is a million, or the buffers of a container type that keeps its capacity. Buffers show under their element type (`typeid(T[])`) with the slot they occupy, since a container's buffer is sized by its capacity, not its size: a `vector` that grew to a million elements and was cleared holds its million-element buffer until it is destroyed or `shrink_to_fit()`.

Then the numbers over time: `get_statistics()` at intervals, from any thread, without waiting:

```cpp
auto s = gc::collector::get_statistics();
std::cout << s.cycles << " cycles (" << s.full_cycles << " full), last " << s.last_cycle_ms << " ms, "
          << s.live_objects << " objects marked, " << s.live_bytes / 1048576 << " MB in use, "
          << s.committed_bytes / 1048576 << " MB committed\n";
```

`live_bytes` is what the allocators hold, garbage not yet swept included; `live_objects` is what the last cycle marked. `live_bytes` growing while `live_objects` does not is garbage waiting for a cycle: the collector runs one every quarter of growth, so a program that allocates fast holds up to a quarter more than it uses ([README: Memory](../README.md#memory)). `live_objects` growing is a leak in the program's sense: something reaches the objects.

### Something reaches an object that should be dead

Four things keep an object alive that the program has let go of, in the order to check them:

1. **A word on a stack.** The stacks are scanned conservatively: a dead local, a spilled register, a temporary the compiler left in a frame keep their target for as long as the frame is not overwritten. This is the first suspect for an object that dies "a little later" than expected, and the reason the tests count after `collector::clear_stack(SIZE_MAX)`. A program does not need to clear its stack; a test that asserts a count does, and asserts it from a frame the cleared region does not include ([collector: clear_stack](collector.md#clear_stack)).
2. **A young cycle.** The marks are sticky through the young cycles: an object marked once stays marked until a full cycle looks again ([README: Generations](../README.md#generations)). `force_collect(true)` is a full cycle; the destructor of an object dropped between full cycles runs at the next one.
3. **A container's capacity.** A `vector` or `array<T>` roots every element up to its `size()`, and an element removed by `pop_back()` or `erase()` is destroyed at once; but a buffer abandoned by a reallocation is garbage of the next cycle, and the elements of a buffer that a dying `vector` leaves behind are collected with it, not destroyed on the spot ([vector: Rules](vector.md#rules)).
4. **A cell of a `gc::tracked_ptr`.** A `gc::tracked_ptr` in unmanaged memory (a `std` container, a global, a lambda on the heap) holds its object for exactly its own lifetime, through a cell in a block of a cache line; a block lives while one of its cells is in use ([gc::tracked_ptr](gc/tracked_ptr.md)). A `gc::tracked_ptr` that outlives its owner's intent, a global or a cached closure, is the pointer to look for.

To see which it is, ask the collector:

```cpp
gc::collector::explain(p, std::cerr);
// 0x1000a0040 is held by
//   a buffer of Item[] at 0x1000c0000, the word at byte 1040
//   a Cache at 0x1000a0100, the word at byte 16
//   a unique_ptr: the Cache at 0x1000a0100 is its object
// and keeps alive 3 objects, 112 bytes, itself included
```

`get_retained(p)` is the last line as data: what would go if the object went, the way a heap profiler reports a dominator. Asked of the top of a chain, of a global's object or a cache, it says whether that holder is the leak or only a link in it.

`get_path_to_root(p)` is the same chain as data, `get_referrers(p)` every word that points at the object, the stack words included ([collector: get_referrers](collector.md#referrer-get_referrers-get_path_to_root-explain)). A chain that ends on a stack word names the thread; one that ends on a word of the calling thread, with nothing else in it, is case 1: nothing but that frame, or a stale word in it, holds the object. A chain through a buffer at a byte past the container's `size()` is case 3. A chain that ends at a `cell` is case 4: a `gc::tracked_ptr` somewhere in unmanaged memory. A `unique_ptr` at the top is an owner the program forgot, a global as a rule. Where the whole picture of a cycle is wanted rather than one object, `-DSGCL_TRACE_STACK` prints every marking decision: which stack word retained which object (`[stack]` and `[scan]` lines), which object was found reachable by its state (`[updated]`), by a card (`[dirty]`) or by a hazard pointer (`[hazard]`); a line per object per cycle, so for a program reduced to the case.

### An object dies too early

With the rules kept, it does not: an object is reachable or it is not. The rules broken are what a debug build asserts, at the point of the break rather than at the crash later:

- `a tracked_ptr must live on the stack or inside a managed object`: an `sgcl::tracked_ptr` (or a container, atomic or weak pointer of `sgcl::`) constructed in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame. The collector cannot see it; its object dies at the next cycle. The fix is the `gc::` type ([README: The two namespaces](../README.md#the-two-namespaces)).
- `a tracked_ptr may address a managed object or a part of it, not an element of a container's buffer`: a `tracked_ptr` made from the address of a `vector` element. A buffer is rooted by the pointer to its first element only; an alias into it keeps nothing. Hold the container, or an index.
- `a weak_ptr cannot address an object a unique_ptr owns`: a `weak_ptr` made before the object was handed to a `tracked_ptr`.
- `type T: the word at byte offset N was classified as data but holds a pointer to a managed object` (a message, once per type, from the collector thread): a `tracked_ptr` sharing its storage with data, in a `union`, a `std::variant`, a small-buffer `std::function` or `std::any`. The collector's pointer map is built by elimination and cannot follow a word that is a pointer in one object and data in another ([README: The rules](../README.md#the-rules), 2).

A `tracked_ptr` read in a destructor is the other case: the destructor of a collected object runs on a collector thread, in no order with the destructors of the objects that die with it, so a member pointing at a peer that dies in the same sweep must be read through `if_alive()` ([tracked_ptr: if_alive](tracked_ptr.md#if_alive)). A crash in a destructor that dereferences a member is this.

Sanitizers find the rest: a build with `-DSGCL_SANITIZER=address` (CMake) or `-fsanitize=address` catches a use after the sweep as a use of freed memory, since a page the collector returns is unmapped or poisoned; `thread` finds a `tracked_ptr` written by one thread and read by another without `atomic`, `atomic_ref` or the program's own synchronization (rule 6). The library's own reads of other threads' stacks and words are annotated, so the reports are the program's.

### What the collector costs

`get_statistics().phases_ms` is the last cycle by phase: registration, states, roots, marking, updated states, sweep, page release, trim (`collector::phase_names`). What to read from it:

- **marking** large and **roots** small: the live heap is large; a full cycle traces all of it. If the program's live set is stable, most cycles should be young ones (`full_cycles` a fraction of `cycles`), whose marking covers the survivors and the dirty pages only ([README: Generations](../README.md#generations)).
- **roots** large: many threads, or deep stacks, or many dirty pages in a young cycle: a program that links old objects more than it allocates stamps a card on every store into an old object, and a dirty page has every marked object on it traced again. `-DSGCL_GENERATIONAL=0` builds a collector of full cycles only, with a barrier that does no carding, for such a program ([config](config.md)).
- **sweep** large: many objects with destructors died at once; the sweep runs them on the pool when there are many pages, on the collector's thread otherwise.
- `last_helpers_used` says whether the pool joined: the helpers start from `config::MarkObjectThreshold` objects and `SweepPageThreshold` pages ([config](config.md)).

The mutators' side is not in the statistics: it is the write barrier on every pointer copy and the allocation, whose costs the [benchmarks](../README.md#benchmarks) give, and which no diagnostic changes.

### At the memory ceiling

`get_memory_limit()` is the ceiling (90% of the cgroup or physical limit by default), `committed_bytes` the distance to it. Near the ceiling the collector runs more often and returns free chunks at once; at it, an allocation forces a full collection and throws `std::bad_alloc` if that was not enough. A program that catches `bad_alloc` from `make_tracked` or a container is at the ceiling with a live set that does not fit: the type statistics say what it is ([collector: get_memory_limit](collector.md#get_memory_limit-set_memory_limit)).

### A race with the collector

A store that lands in a particular window of a cycle, an object made after the flip of the epoch, a pointer read while the weak cells are cleared: the stress tests find these by luck, `collector::stepper` finds them on purpose. A test takes the collector and lets it through one gate at a time, doing the mutator's work in between:

```cpp
gc::collector::stepper s(false);                       // young cycles; the test's thread is the mutator
gc::tracked_ptr holder = gc::make_tracked<Node>();
s.finish_cycle();                                      // holder is old and marked
s.advance_to(gc::collector::stepper::phase::roots);    // the stacks scanned, the dirty pages traced
holder->next = gc::make_tracked<Node>();               // stored into an old object after the trace: the card
s.finish_cycle();                                      // not swept: made after the flip
s.finish_cycle();                                      // registered now, found through the card
```

The gates and the scenarios the library's own tests assert with them are in [collector: stepper](collector.md#stepper). What a program's tests would use it for: a data structure of its own that stores pointers across threads, checked at every gate rather than under a loop that hopes to hit the window.

### In the debugger

`lldb/sgcl.py` is a set of LLDB formatters for the pointers and containers of both namespaces. Loaded once (`command script import <sgcl>/lldb/sgcl.py`, in `~/.lldbinit` for every session; Xcode and the VS Code extension pick it up from there), `frame variable` and the variables view show:

```
(gc::tracked_ptr<Node>) node = 0x10000270000 (tracked, Reachable Fresh) {
  object = { v = 7, next = 0x10000270010 (tracked, Reachable Fresh) { ... } }
}
(gc::tracked_ptr<Node>) kept = 0x10000270010 (cell 0 of block 0x10000260000, Reachable Fresh) { ... }
(sgcl::unique_ptr<Node>) owned = 0x10000270020 (UniqueLock) { object = { v = 7, next = null } }
(sgcl::weak_ptr<Node, gc::tracked_ptr>) weak = 0x10000270000 { object = { ... } }
(gc::vector<int>) v = size=3 capacity=4 { [0] = 1, [1] = 2, [2] = 3 }
(gc::map<int, std::string>) m = size=2 { [0] = (first = 1, second = "one"), [1] = (first = 2, second = "two") }
```

A pointer shows its address, for a `gc::tracked_ptr` its mode (`tracked`, or the cell and the block that holds it), and, when the collector's types are in the debug info, the state of the object's slot (`Reachable` with the parity, `UniqueLock`, `Destroyed`: [how it works](how-it-works.md#an-objects-slot)); its one child is the object. A `weak_ptr` shows its target while the cell holds it, `expired` after. A container shows its size and its elements, read from the managed buffer or walked node by node, whatever the namespace and the kind of its root word. The expression evaluator does not know the inline operators (`p node->v` fails); `frame variable node.object.v` reads through the formatter, and `p node.get()->v` calls what is compiled in. `lldb/check.sh` builds `lldb/example.cpp` and checks the output; GDB has no counterpart yet.

### Reading the log

`-DSGCL_LOG_PRINT_LEVEL=2` prints one line per cycle on `std::cout`: the pages allocated and freed since the last cycle, the total, the objects created and removed, the live objects, the kind of the cycle (`full`/`young`), the helpers used, the time. A program whose memory grows shows it there as `objects removed` staying below `objects created` cycle after cycle; a program that stalls on allocation shows cycles back to back. Level 1 prints the thread events and every `force_collect()`, level 3 the collector's pauses for `get_live_objects()` ([config: SGCL_LOG_PRINT_LEVEL](config.md#sgcl_log_print_level)).
