# SGCL
## About SGCL
SGCL (Smart Garbage Collection Library) is a concurrent, generational garbage collector for C++20 in a header-only library. It gives the language what Go and Java have and `shared_ptr` does not: objects that live as long as anything reaches them, cycles included, allocated and passed around without reference counts, with a collector that runs concurrently with the program. What it keeps from C++ is the rest: deterministic destruction where it is wanted (`unique_ptr`), objects that never move, stack objects and raw pointers as they are, containers with the interfaces of `std`, and no runtime beyond the library itself.

The point of the design is that the program never stops for the collector. There is no stop-the-world phase, no safepoint a thread has to reach, no handshake in the hot path, no allocation that waits for a cycle and no write barrier that does more than store a byte. A mutator thread runs at the same speed whether the collector is idle or in the middle of a cycle; the collector and its helpers take cores of their own and the memory that accumulates between two cycles. That trade is stated exactly in the benchmarks below, against `shared_ptr`, Go and Java with ZGC.

The library comes in two namespaces with one interface. `gc::` is the one to start with: `gc::tracked_ptr<T>` is the pointer, `gc::make_tracked<T>(...)` creates the object, and `gc::vector`, `gc::map`, `gc::weak_ptr`, `gc::atomic`, `gc::task` and the rest are the containers, observers and coroutines, all of which may live anywhere a C++ object may, a `std::vector`, a global or a lambda on the heap included. `sgcl::` is the same interface under the same names, faster by a fraction of a nanosecond per operation, with one restriction: its types live inside managed objects and on stacks only, never in unmanaged memory, which is where the collector cannot see them. Code that keeps its pointers in managed objects and on stacks, a node structure, a hot loop, takes `sgcl::`; everything else takes `gc::`, and the two convert into each other. The examples below use `gc::`; the tables at the end give both columns.

## Next to the alternatives
| | SGCL | `shared_ptr` / `unique_ptr` | Go | Java, ZGC |
|---|---|---|---|---|
| Pauses, safepoints | none: no thread is ever stopped or asked to reach a point | none | short stop-the-world phases, preemption at safepoints | short pauses at phase changes, safepoints |
| What a mutator waits for | nothing in the collector: a page from the heap under a mutex (once per 64 KB), the memory ceiling | the destructor cascade of what it releases | allocation assists when the collector is behind | allocation stalls when the collector is behind |
| Pointer copy | a store and a byte of state, 1.4 ns onto the stack and 1.8 into an object (the card), the same when a thread copies a shared object's pointer | a reference count update, 5 ns alone and 100–300 ns on a shared object | a store, 0.6 ns, plus the barrier while marking | a store and a load barrier, 1 ns |
| Allocation | 5 ns, a per-thread bitmap, no lock | 21 ns, malloc | 7 ns, assists included | 3 ns, TLAB |
| Cycles | collected | leak unless broken by hand | collected | collected |
| Objects | never move | never move | never move | relocated, with load barriers |
| Heap | precise, through pointer maps the collector builds itself | | precise, stack maps from the compiler | precise |
| Stacks | conservative | | precise | precise |
| Destructors | deterministic through `unique_ptr`, otherwise on the collector's threads | deterministic | finalizers | cleaners |
| Weak pointers | `weak_ptr`, one word, cleared by the cycle that finds the object unreachable | `weak_ptr`, a second count | `weak.Pointer` | `WeakReference` |
| Generations | young cycles with sticky marks and cards, full cycles on a schedule; full cycles only as an option | | none (a non-generational collector by design) | young and old, ZGC generational |
| Memory | a cycle every quarter of growth; no throttling, the memory grows when the program outruns the collector | exact | kept near `GOGC` by throttling the mutators | kept under the heap ceiling by stalling the mutators |
| Runtime | this header-only library | the standard library | the Go runtime | the JVM |

## How it works
The collector is a concurrent, non-moving, generational mark-and-sweep with a Dijkstra insertion barrier. A cycle begins with a flip of the epoch, one atomic store that retires every state the barrier set before it (a state carries the parity of the epoch it was set in), and the registration of the objects created before the flip; the roots are the used pages of every registered thread's stack, scanned conservatively while the threads keep running, plus the objects a `unique_ptr` owns; the heap is traced precisely, through a map per type of the words that may hold a pointer, which the collector builds by elimination as it goes (below, "Pointer maps"). Marking runs until a pass over the pages finds no unmarked object with a state the barrier set meanwhile; then the unmarked registered objects are swept, their destructors run, and their slots and pages go back to the allocators and the heap.

What the mutators do for all this is small and never blocking. Creating an object is a bitmap pop in a per-thread allocator and one byte of state. Copying a pointer into an object or onto a stack is the store of the word and the barrier: a byte of state on the target, a flag on its page, both conditional, so that threads copying pointers to the same object do not fight over a line. Nothing registers a constructor, nothing counts references, nothing is looked up. A thread that has never touched the library is registered the first time it copies a pointer, with a thread-local flag; when it exits it waits at most for a stack scan in progress to finish reading its stack. That handshake and the mutex a thread takes once per 64 KB page it gets from the heap are the only places a mutator can wait on anything, and neither involves the collector's work.

The collector side scales with cores instead. Marking runs on a pool of helper threads, each tracing from a stack of its own and stealing from the others, once a cycle has enough to mark; the passes over the pages (registration, the search for barrier states, the sweep with its destructors, the rebuild of free bitmaps) are split over the same pool once the collector falls behind the allocation. The states of eight slots are read and tested at once. A cycle starts when the pages allocated since the last one reach a quarter of what it left in use, so what waits for a sweep is bounded by the live heap; the collector never throttles a mutator to keep that bound, which is where its memory goes above Go's on a program that allocates faster than any collector sweeps (binary-trees with four mutators: 495 MB against Go's 224 MB, in half the time).

The cycles are generational by default (sticky mark bits): a young cycle traces only the objects created since the previous cycle and the old objects a pointer was stored into since, which the barrier records by stamping a card of the page written to (a shift and a byte read); a full cycle runs every eighth cycle, when the live heap has doubled, under memory pressure and on request. `-DSGCL_GENERATIONAL=0` builds a collector with full cycles only and a barrier without the card.

## Features
- **No reference counts**: a `tracked_ptr` is one word, copied with a store and a byte of state; the objects it points to may form any graph, cycles included.
- **Two families, one interface**: `gc::` lives anywhere, `sgcl::` lives in managed objects and on stacks and is the faster of the two; the same names, the same members, conversions both ways.
- **No pauses**: no stop-the-world, no safepoints, no assists, no allocation that waits for a cycle; the tails of a mutator's latency are the scheduler's, not the collector's (the graph benchmark below).
- **Deterministic where it matters**: `unique_ptr` destroys its object at scope exit, on the thread that owns it; a `tracked_ptr` hands the destructor to the collector.
- **Containers**: `vector`, `array`, `deque`, `list`, `forward_list`, the maps and sets, ordered and unordered, with the interfaces of `std`, their nodes and buffers managed.
- **Lock-free atomic pointers**: `atomic<tracked_ptr<T>>` and `atomic_ref` with compare-exchange, and no ABA: a node is never reused while a thread holds it.
- **Coroutines**: a promise type derived from `managed_frame` gets its frames from the managed heap, so the `tracked_ptr` locals, parameters and promise members of a suspended coroutine are roots; `task<T>` and `generator<T>` come ready.
- **Weak pointers**: `weak_ptr<T>` with `lock()` and `expired()`, one word, copied for the price of a `tracked_ptr`; cleared by the collector, never dangling, never a reference count. `expiry_queue<T>` hands an object found unreachable to a function of your choice, on a thread of your choice, alive one last time: a cleanup, or a return to life.
- **Dynamic type**: `type()`, `is<U>()` and `as<U>()` on any pointer, including `tracked_ptr<void>`, without virtual functions.
- **Diagnostics**: cycle counters and phase times, live objects and bytes by type, the live objects themselves; read without stopping the collector or after a cycle it runs for the question ("Methods useful for state analysis").
- **Memory under control**: a committed-memory ceiling (90% of the cgroup or physical limit by default), a collection forced before it and `std::bad_alloc` instead of the OOM killer past it; the whole managed heap is one reservation, backed lazily and returned in 2 MB chunks.

## Reference
Every public class and function has a page of its own in [docs/](docs/README.md): every member with its signature, the rules that apply, and examples that compile. The sections below are the guide; the reference is where to look up a member, and [docs/diagnostics.md](docs/diagnostics.md) is where to start when the memory grows, an object lives too long or dies too early, or a cycle costs more than it should.

## The two namespaces
`sgcl/sgcl.h` declares both namespaces (`gc/gc.h` declares `gc` and includes `sgcl`). Every name below exists in both; what differs is the word by which a type holds its memory.

- In `sgcl::`, that word is `sgcl::tracked_ptr<T>`: the pointer the collector follows, one word, a store and a byte of state per copy. The collector finds such words in two places only, inside managed objects and on the stacks it scans, so an `sgcl::tracked_ptr`, and every `sgcl` container, atomic, weak pointer or coroutine handle, lives there and nowhere else: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a lambda copied to the heap (rule 1 below). Debug builds assert it.
- In `gc::`, that word is `gc::tracked_ptr<T>`: inside a managed object or on a stack it is an `sgcl::tracked_ptr`, the same word at the same cost; in any other memory it is the address of a cell, a word of a managed block of a cache line of them that is the object's root, taken by the constructor from the thread's allocator, given back by the destructor, the pointer's own in between: no store allocates and no move takes a cell from another pointer, so threads race on the cell's word exactly as on an `sgcl::tracked_ptr`, never on the making of a cell; a block is one managed allocation per sixteen cells and is freed by the collector once every cell of it is given back. The mode is decided by the address at construction and read from the sign of the word; the collector never sees the cell form. What a `gc` type pays for living anywhere is the test of that word on each access: a store into a local 1.5 ns against 1.3, into a member 1.9 against 1.8, a dereference 0.48 against 0.44, a construction on the stack 1.8 against 1.3 (the check of the address), a construction and destruction in unmanaged memory 7.4 ([docs/gc/tracked_ptr.md](docs/gc/tracked_ptr.md), and the `gc::` columns of the benchmarks below).

`gc::vector<T>` is `sgcl::vector<T, gc::tracked_ptr>`: each container, `expiry_queue`, `task` and `generator` takes the kind of its root word as its last template parameter, `sgcl::tracked_ptr` by default, and the `gc` names are aliases with `gc::tracked_ptr` in that place. `gc::weak_ptr<T>` is `sgcl::weak_ptr<T, gc::tracked_ptr>`; `gc::atomic`, `gc::atomic_ref`, `gc::unique_ptr`, `gc::make_tracked` and `gc::collector` are the `sgcl` ones, which need no other kind. A `gc` container and an `sgcl` one of the same element type hold the same managed nodes and buffers; the elements are a choice of their own, and in a managed buffer a `gc::tracked_ptr` is an `sgcl::tracked_ptr`.

## The classes
Under either namespace:

- `unique_ptr<T>`: what `make_tracked<T>(...)` returns. A specialization of `std::unique_ptr` whose object lives on the managed heap: destroyed at scope exit like any `unique_ptr`, and the root of whatever it owns meanwhile; lives anywhere. Converts into a `tracked_ptr`, after which the object belongs to the collector.
- `tracked_ptr<T>`: the pointer the collector follows. Copies, converts to base classes, compares, `reset()` and `reset(T*)`, `get()`; `type()`, `is<U>()` and `as<U>()` for the dynamic type of the object; `if_alive()` for the one situation a pointer may be dangling, a destructor reading a peer that may be dying in the same sweep ("Pointer maps" below); `sgcl::tracked_ptr` also has `to_shared()`, a `std::shared_ptr` that holds the object from unmanaged memory, from before `gc::` existed ("The rules" below).
- `atomic<tracked_ptr<T>>` and `atomic_ref<tracked_ptr<T>>`: `load`, `store`, `compare_exchange_weak` and `compare_exchange_strong` with `std::memory_order`, plus `wait`/`notify`; lock-free, the loaded object protected by a hazard pointer for the length of the load. A global shared pointer is `static gc::atomic<gc::tracked_ptr<T>>`.
- `managed_frame`, `frame_ptr<Promise>`, `task<T>`, `generator<T>`: coroutines with frames on the managed heap ("Coroutines" below); `gc::task` and `gc::generator` are what a scheduler keeps in a `std::vector`.
- `expiry_queue<T>`: `watch(object, f)` is a `weak_ptr` to the object plus `f`, kept for the day nothing else reaches the object; `drain()` calls the `f` of every such entry with the object, alive one last time ("Weak pointers" below).
- `weak_ptr<T>`: a pointer that keeps nothing alive. `lock()` is the object as a `tracked_ptr` while it is reachable and null once a cycle has found it unreachable; `expired()`, `reset()`; made from a `tracked_ptr` of either kind or another `weak_ptr` ("Weak pointers" below).
- The containers, listed below.

## Containers
`vector`, `array`, `deque`, `list`, `forward_list`, `map`, `set`, `multimap`, `multiset`, `unordered_map`, `unordered_set`, `unordered_multimap`, `unordered_multiset` and the adapters `stack`, `queue`, `priority_queue`, in `gc::` and in `sgcl::`, follow the interfaces of their `std` namesakes, including iterator categories (`std::ranges` algorithms work on them), transparent lookup, node handles, `std::erase`/`std::erase_if`, and three-way comparison. They differ from the standard containers in where their memory lives and when elements die:

- A `gc` container lives anywhere; an `sgcl` container holds its buffer or its root node by an `sgcl::tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory or in a standard container ("The two namespaces" above). Iterators are plain pointers, valid exactly when their `std` counterparts are, and may live anywhere: the container roots every element it holds, and a raw pointer in a stack frame is a root of its own under the conservative scan.
- Nodes and buffers are managed objects: an `erase` unlinks a node and the collector reclaims it later; nothing is ever freed by hand, so a cycle through a container is collected like any other cycle.
- The node containers (`list`, `forward_list`, the maps and sets) destroy an element the moment it is erased, cleared, assigned over or the container is destroyed, exactly like `std`. An iterator to an erased element is invalid as in `std`; it keeps the node's memory mapped but not the element. The one exception is a container dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it.
- `vector` and `array` destroy their elements themselves, exactly when `std` does: on removal (`erase`, `pop_back`, `clear`, `resize`, `assign`), on a reallocation (the moved-from elements), in the destructor, wherever that runs, on a stack or in a sweep inside a dying managed object. The collector never destroys a buffer: it only frees one nothing refers to, and it never needs to know how many elements a buffer holds. A buffer is referred to only through a pointer to its first element: a `tracked_ptr` or reference to an element does not keep it, so once the container is gone such a pointer dangles, as in `std`. `clear()` keeps the capacity, like `std::vector`; `shrink_to_fit()` on an empty vector drops the buffer. A `vector` is three words (the buffer, the count, the capacity), an `array<T>` two; the buffer's own header holds only its metadata and the capacity the size class granted.
- `array<T, N>` keeps its elements inline, like `std::array`: an aggregate (`gc::array<gc::tracked_ptr<T>, 4> roots = {}`) with the tuple interface and costs nothing beyond the elements. `array<T>` (no `N`) is a buffer whose size is fixed when it is created (`array<T>(n)`, `array<T>(n, value)`, from a range or an initializer list): the cheapest managed sequence, a single word to hold, copied deeply and moved by handing the buffer over.
- Elements aligned beyond 16 bytes are not supported in buffers; `vector<bool>` is a plain vector of `bool`.

## make_tracked
`sgcl::make_tracked<T>(args...)` creates an object on the managed heap and returns a `unique_ptr<T>`: deterministic until it is converted into a `tracked_ptr` or dropped. Managed arrays are not a public type: `sgcl::vector` and `sgcl::array<T>` own their buffers on the managed heap.

## Pointer aliases
A `tracked_ptr` may point into the middle of a managed object: to a member or to a base subobject. Such an alias behaves like the aliasing constructor of `std::shared_ptr`: the object it points into stays alive for as long as the alias does.

```cpp
struct Item { int value; std::string name; };
gc::tracked_ptr item = gc::make_tracked<Item>();
gc::tracked_ptr<int> alias(&item->value);
item = nullptr;                    // the Item lives on: the alias keeps it
```

What a `tracked_ptr` may not address is an element of a container's buffer (`sgcl::vector`, `sgcl::array<T>`): a buffer is rooted only through the pointer to its first element that the container holds, an alias into it would keep nothing, and debug builds assert on the attempt. A pointer or reference to an element of any `sgcl` container is exactly as valid as with its `std` counterpart: until the element is removed, the container reallocates, or the container is destroyed.

## Weak pointers
`weak_ptr<T>` is a pointer the collector does not follow: the object lives as long as something else reaches it, and `lock()` says which. It is one word, a `tracked_ptr` to a small cell on the managed heap that holds the target as a word the collector clears instead of tracing; a `weak_ptr` made from a `tracked_ptr` gets a cell of its own, copies share it, and the cell is collected with the last copy. It lives wherever a `tracked_ptr` may, and threads share it the way they share a `tracked_ptr` (rule 6).

```cpp
struct Item { std::string name; };
gc::tracked_ptr item = gc::make_tracked<Item>("x");
gc::weak_ptr cached = item;                  // a cell, allocated once
if (auto p = cached.lock()) {                  // the Item, held by p
    p->name = "y";
}
item = nullptr;                                // unreachable now
gc::collector::force_collect(true);          // optional, for the demonstration only: the next cycle clears it anyway
assert(cached.expired() && !cached.lock());    // cleared, never dangling
```

The clearing is a phase of the cycle: once the marking has converged, every cell whose target the cycle found unreachable has its word cleared, before the sweep. So `lock()` never hands out an object the sweep will destroy or the slot it will be reused for; and when `lock()` races with the clearing it either sees the null or wins, holding the object for at least one more cycle, through the same hazard pointer as `atomic<tracked_ptr>::load` (a cell is cleared before the collector reads the hazards, a lock publishes its hazard before it reads the cell again). `expired()` is true once the cell is cleared; between the object becoming unreachable and the cycle that notices, `lock()` still returns it, which is the same lag as any garbage collector's. A program without weak pointers pays nothing: the phase is a test of an empty list.

Nanoseconds per operation against `std::weak_ptr`, Go's `weak.Pointer` and Java's `WeakReference`, one thread and four threads on objects of their own (`benchmarks/weak_ptr.cpp` and its Go and Java counterparts, the setup of the "Benchmarks" section):

| operation, threads | `sgcl::weak_ptr` | `gc::weak_ptr` | `std::weak_ptr` | Go `weak.Pointer` | Java `WeakReference` |
|---|---|---|---|---|---|
| lock, 1 | 2.2 | 3.2 (1.48×) | 12.3 | 6.1 | 1.0 |
| lock, 4 | 2.3 | 3.4 (1.50×) | 12.9 | 6.2 | 1.2 |
| copy, 1 | 1.4 | 2.1 (1.52×) | 8.6 | 6.3 | 0.8 |
| copy, 4 | 1.5 | 2.2 (1.50×) | 8.7 | 6.4 | 0.9 |
| make from a strong pointer, 1 | 7.6 | 9.6 (1.25×) | 8.7 | 18.0 | 3.2 |
| make from a strong pointer, 4 | 8.1 | 10.2 (1.26×) | 8.9 | 18.6 | 7.1 |

A lock is the cell read twice around a hazard store and a `tracked_ptr` built; a copy is a `tracked_ptr` copy; making one is a 16-byte allocation. A `gc::weak_ptr` is the same word as a `gc::tracked_ptr` and pays the same: the location check when it is built or copied (the copy here is a copy onto the stack, then `expired()`, which reads the cell through the test of the sign), the test of the sign when its cell is read; the gap of the copy is that check. `std::weak_ptr` pays two atomic count updates per lock and per copy, and contended ones when threads share an object. Go's `weak.Pointer` is a handle the runtime hands out and resolves in a call (`Value`), and making one allocates the handle; Java's `WeakReference` is an object whose referent is read through ZGC's load barrier, the cheapest lock of the four, and making one is an allocation the collector has to discover.

### expiry_queue
A destructor is the object's own business and runs on the collector's threads under the rules of destructors; what an observer wants done with an object once nothing else reaches it (a handle released, a cache entry dropped, a registry told, or the object kept after all) goes into an `expiry_queue<T>`. `watch(object, f)` hands out a `weak_ptr` to the object and keeps `f` next to it. When a cycle finds the object unreachable it does not destroy it: it keeps it alive for the queue, and `drain()` calls `f` with the object as a `tracked_ptr`, alive one last time, on the thread that calls `drain()`, at that moment, with the heap in a consistent state. `f` may read the object, release what it owns, or keep the pointer, which is the object's return to life (it can be watched again). Then the entry is dropped and the object dies with the next cycle that finds it unreachable, its destructor as ever. Until `drain()` the object stays alive, and its `weak_ptr`s lock it; the queue drains by itself every so many `watch` calls, as many as it has entries, and a thread that watches little and wants its cleanups on time calls `drain()` in its loop. What `f` captures follows rule 1 (a `std::function` keeps its closure on the unmanaged heap); the object comes as the argument. Java's `Cleaner` and Go's `AddCleanup` do the cleanup on a thread of the runtime's and never show the object; here the program says where and when, and gets the object.

```cpp
struct Texture { GLuint id; };
gc::expiry_queue<Texture> gone;                          // lives anywhere, as every gc type

gc::tracked_ptr texture = gc::make_tracked<Texture>(upload(pixels));
gc::weak_ptr weak = gone.watch(texture, [](gc::tracked_ptr<Texture> t) { glDeleteTextures(1, &t->id); });
// ... the texture is used, shared, dropped by everyone
gone.drain();                                            // in the render loop: the GL name freed on this thread, the object destroyed by a later cycle
```

The cost, for a program without such queues, is a test of an empty list per cycle; with them, a pass over the cells per convergence of the marking and one more round of marking for the objects kept, plus their memory until the drain.

## Examples
The basics, in one file (`examples/example.cpp` has the long version):

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <memory>
#include <vector>

struct Node {
    int value;
    gc::vector<gc::tracked_ptr<Node>> edges;   // any graph, cycles included
};

int main() {
    // make_tracked returns a unique_ptr: destroyed at scope exit, deterministically
    gc::unique_ptr unique = gc::make_tracked<int>(42);
    auto also_unique = gc::make_tracked<int>(2);   // the same type, deduced

    // A tracked_ptr hands the object to the collector: destroyed when unreachable
    gc::tracked_ptr tracked = gc::make_tracked<int>(24);
    tracked = std::move(unique);                   // the 42 now belongs to the collector

    // A cycle, collected like anything else
    gc::tracked_ptr a = gc::make_tracked<Node>(1);
    gc::tracked_ptr b = gc::make_tracked<Node>(2);
    a->edges.push_back(b);
    b->edges.push_back(a);
    a = b = nullptr;                               // garbage, no leak

    // Base classes and the dynamic type
    struct Shape { virtual ~Shape() = default; };
    struct Circle : Shape { double r = 1; };
    gc::tracked_ptr<Shape> shape = gc::make_tracked<Circle>();
    if (shape.is<Circle>()) {
        gc::tracked_ptr<Circle> circle = shape.as<Circle>();
        std::cout << "a circle of radius " << circle->r << '\n';
    }
    gc::tracked_ptr<void> any = shape;             // type() still knows: Circle

    // An alias into a member keeps the whole object
    gc::tracked_ptr node = gc::make_tracked<Node>(7);
    gc::tracked_ptr<int> value(&node->value);
    node = nullptr;
    std::cout << *value << '\n';                   // 7, the Node lives on

    // Containers with the interfaces of std, their nodes and buffers managed
    gc::unordered_map<std::string, gc::tracked_ptr<Node>> index;
    gc::list<int> numbers = {1, 2, 3};
    gc::vector<gc::tracked_ptr<Node>> nodes(10);

    // A gc pointer or container lives anywhere: a std container, a global,
    // new memory. The sgcl:: ones may not ("The two namespaces" above).
    std::vector<gc::tracked_ptr<Node>> kept = {value.as<Node>()};   // fine: a cell roots the Node
    static gc::tracked_ptr<Node> root = nodes[0];                   // fine: a global
    auto holder = new gc::vector<gc::tracked_ptr<Node>>(nodes);     // fine: the vector's root word is a cell
    // std::vector<sgcl::tracked_ptr<Node>> edges;                  // not allowed: never scanned, the object is lost
    // static sgcl::tracked_ptr<Node> root;                         // not allowed: a global is neither a stack nor an object
    delete holder;
}
```

The frame of a coroutine is heap memory too. A `gc::tracked_ptr` among its parameters, locals or promise is fine in any frame; an `sgcl::tracked_ptr` is allowed only when the promise derives from `managed_frame`, which `task` and `generator` do, and a managed frame costs nothing at each use of the pointer ("Coroutines" below):

```cpp
std::generator<gc::tracked_ptr<Node>> chain(int count);      // a plain frame: each gc pointer in it roots its Node through a cell
gc::generator<gc::tracked_ptr<Node>> chain(int count);       // a managed frame: the frame itself is a managed object, no cells
// std::generator<sgcl::tracked_ptr<Node>> chain(int count); // not allowed: a plain frame is never scanned
```

Threads, a lock-free structure and a destructor, which is where the collector differs from the alternatives (`examples/threads.cpp`):

```cpp
#include "sgcl/sgcl.h"
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

// A graph built by several threads while the collector runs: nothing is
// counted, no thread waits for a cycle, and cycles in the graph are fine.
struct Node {
    explicit Node(int id) : id(id) {}
    ~Node() {
        // A destructor runs on a collector thread. It reads the tracked_ptr
        // members of its object only through if_alive(): a copy when the
        // target is alive, null when it is dying in the same sweep.
        if (auto p = peer.if_alive()) {
            ++p->orphaned;
        }
    }
    int id;
    int orphaned = 0;
    gc::vector<gc::tracked_ptr<Node>> edges;   // any graph, cycles included
    gc::tracked_ptr<Node> peer;
};

// A lock-free stack: an atomic tracked_ptr, no ABA (a node is never reused
// while a thread holds a pointer to it) and no hazard pointers to manage.
template<class T>
class Stack {
    struct Item {
        explicit Item(T v) : value(std::move(v)) {}
        T value;
        gc::tracked_ptr<Item> next;
    };
    gc::atomic<gc::tracked_ptr<Item>> _head;

public:
    void push(T v) {
        gc::tracked_ptr item = gc::make_tracked<Item>(std::move(v));
        item->next = _head.load();
        while (!_head.compare_exchange_weak(item->next, item)) {}
    }
    std::optional<T> pop() {
        auto item = _head.load();
        while (item && !_head.compare_exchange_weak(item, item->next)) {}
        if (item) {
            return std::move(item->value);
        }
        return std::nullopt;
    }
};

int main() {
    Stack<gc::tracked_ptr<Node>> ready;
    std::vector<std::thread> workers;
    for (int t = 0; t < 4; ++t) {
        // A thread registers itself the first time it copies a managed pointer
        workers.emplace_back([&ready, t] {
            for (int round = 0; round < 1000; ++round) {
                gc::tracked_ptr root = gc::make_tracked<Node>(t);
                gc::tracked_ptr<Node> prev = root;
                for (int i = 1; i < 100; ++i) {
                    gc::tracked_ptr node = gc::make_tracked<Node>(i);
                    node->peer = prev;
                    prev->edges.push_back(node);
                    prev = node;
                }
                prev->edges.push_back(root);   // a cycle: collected all the same
                if (round % 100 == 0) {
                    ready.push(root);          // one graph in a hundred is kept
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    // The other graphs are garbage by now, reclaimed by the collector
    // without anyone having stopped for it.
    size_t kept = 0;
    while (auto root = ready.pop()) {
        ++kept;
    }
    std::cout << kept << " graphs kept\n";
}
```

Four threads build a hundred graphs each, with cycles, while the collector reclaims the ones that are dropped; no thread stops for it, and the stack handed to the main thread needs no hazard pointers or reference counts to be safe. The destructor of a `Node` may run on any collector thread, in any order with respect to the peers it points at; `if_alive()` is what makes touching a peer safe.

## The rules
Everything the collector relies on, in one place; the sections below say why.

1. An `sgcl::tracked_ptr`, and every `sgcl` type that holds one (a container, an atomic, a weak pointer, a coroutine handle), lives inside a managed object or on a stack: never in `new`/`malloc` memory, a standard container, a global, a `thread_local`, a lambda copied to the heap, or the frame of a coroutine, unless the coroutine's promise derives from `managed_frame` ("Coroutines" below). Debug builds assert it. The `gc` types live anywhere: a `gc::tracked_ptr` is an `sgcl::tracked_ptr` where one may live and owns a managed cell elsewhere ("The two namespaces" above).
2. A `tracked_ptr` does not share storage with data: no `union` with a value, no `std::variant`, no small-buffer `std::function` or `std::any` holding one. A union of two `tracked_ptr`s and `std::optional<tracked_ptr<T>>` are fine.
3. A raw pointer, a reference or an iterator keeps nothing alive by itself; it is valid while a `tracked_ptr` or a container keeps its target, as with `std`.
4. A `tracked_ptr` addresses a managed object or a part of it (a member, a base), never an element of a container's buffer. A `weak_ptr` follows the rules of a `tracked_ptr` (it is one, to a cell) and addresses an object no `unique_ptr` owns.
5. A destructor reads its object's `tracked_ptr` members only through `if_alive()`; its `unique_ptr` members it may use freely.
6. Objects shared between threads are shared the way any C++ objects are: a `tracked_ptr` written by one thread and read by another needs `atomic` or `atomic_ref`, or the program's own synchronization. The word itself is atomic, so a race on it is never a torn pointer, and the collector is correct under any interleaving.

A global root is a `unique_ptr`: the object it owns is reachable, and so is everything reachable from it, for as long as the global lives. When the global has to point at an object that other threads share and that is replaced at run time (a current configuration, a snapshot), a `unique_ptr` is the wrong shape, since assigning it destroys the old object at once, under the threads still using it; the root is then a `gc::tracked_ptr`, or an `atomic<gc::tracked_ptr>` when the replacement races with the readers. Readers `load()`, a writer `store()`s, and the old configuration is collected when the last reader drops it:

```cpp
static gc::atomic<gc::tracked_ptr<Config>> current;           // the root, for the life of the program: a gc pointer lives in a global

gc::tracked_ptr<Config> config = current.load();                  // a reader: held until dropped
current.store(gc::make_tracked<Config>(...));                // a writer: the old one lives on for its readers
```

The same in `sgcl::` alone is a `unique_ptr` to a managed object holding the atomic: `static sgcl::unique_ptr current = sgcl::make_tracked<sgcl::atomic<sgcl::tracked_ptr<Config>>>();`, read and written through `current->`.

A managed object held from anywhere else in unmanaged memory (a `std::vector`, a `new`ed object, a lambda run on another thread) is a `gc::tracked_ptr`, or a `std::shared_ptr` from `to_shared()` where the holder has to be a `shared_ptr`: its control block owns a managed holder of the pointer, a root that lives exactly as long as the last copy of the `shared_ptr`, and the object stays managed, destroyed on the collector's threads once nothing reaches it. Two allocations per call, so a named function, not a conversion:

```cpp
std::vector<gc::tracked_ptr<Node>> kept;              // a std container: no sgcl::tracked_ptr may live in it
sgcl::tracked_ptr node = sgcl::make_tracked<Node>();
kept.push_back(node);                                 // a gc::tracked_ptr: a cell on the managed heap, the Node's root
std::vector<std::shared_ptr<Node>> shared;
shared.push_back(node.to_shared());                   // or a shared_ptr: the Node lives while it does
```

## Threads
Any thread may create and copy managed pointers; the library registers a thread's stack the first time the thread copies one, and forgets it when the thread exits, with a handshake so that a scan in progress finishes reading the stack first. The collector runs on a thread of its own, started with the first managed object, and on a pool of helpers that park between cycles; destructors of collected objects run on those threads, so a destructor must be prepared to run on a thread other than the one that created the object, and must not touch the dying peers of its object (rule 5). Nothing a mutator does waits for a cycle, and nothing a cycle does waits for a mutator: a thread blocked in a system call, spinning, or descheduled holds up no one; a thread that exits mid-scan waits for the scan of its own stack only.

A `fork()` gives the child a copy-on-write snapshot of the managed heap and none of the threads: the child may read managed objects and `exec` or exit (the page headers live outside the pages, so the parent's marking does not copy the snapshot page by page), but the collector does not run in it, and a managed allocation that needs a page or a call into the collector terminates the child with a message rather than hang on a copied lock. A child that needs the collector is a `fork` before the first managed object, or an `exec`.

## Coroutines
The frame of a C++20 coroutine, where its parameters, locals and promise live between suspensions, is allocated with `operator new`: heap memory the collector does not see, so a `tracked_ptr` in a coroutine breaks rule 1 and its object may be collected under it. A promise type that derives from `sgcl::managed_frame` gets its frames from the managed heap instead: `operator new` of the promise allocates the frame as a buffer of words on the managed heap, and the collector traces such a buffer conservatively, every word that holds a managed address keeping its object, so whatever the coroutine holds is a root while its frame is held. The frame is held through a `frame_ptr<Promise>` (a `tracked_ptr` to the frame and the coroutine handle, move-only, destroys the coroutine when destroyed) that the promise's `get_return_object` makes from the handle. `task<T>` and `generator<T>` are two such coroutine types, complete enough to use and small enough to copy for a scheduler of your own:

```cpp
struct Node { int value; gc::tracked_ptr<Node> next; };

gc::generator<gc::tracked_ptr<Node>> chain(int count) {
    gc::tracked_ptr<Node> last;                 // a local in the frame: a root while suspended
    for (int i = 0; i < count; ++i) {
        gc::tracked_ptr n = gc::make_tracked<Node>(i, last);
        last = n;
        co_yield n;                               // suspended here, the chain is alive
    }
}

gc::task<int> sum(gc::tracked_ptr<Node> head) {   // a parameter copied into the frame: a root too
    int s = 0;
    for (auto n = head; n; n = n->next) {
        s += n->value;
        co_await std::suspend_always{};           // cooperative: resume() continues
    }
    co_return s;
}

for (auto& n : chain(5)) { /* the generator's frame holds the chain */ }
gc::task<int> t = sum(head);
while (!t.done()) t.resume();
int s = t.result();                               // or the exception the coroutine threw
```

The rules that follow: a `task` or `generator` (a `frame_ptr`) lives where a `tracked_ptr` may, on a stack, in a managed object, or in another managed frame; the coroutine is destroyed when its `frame_ptr` is, which runs the destructors of its locals and promise on the calling thread, and the frame's memory goes to the collector once nothing holds it, so no `coroutine_handle` may outlive the `frame_ptr`. The cost is the allocation of the frame as a managed buffer (a few tens of nanoseconds instead of `malloc`) and, per cycle, a conservative pass over the frame's words; a frame that holds no managed pointers costs the collector the same pass and nothing else.

## Pointer maps
The collector finds the pointers inside managed objects on its own; constructors do not register anything, and creating a `tracked_ptr` is a store of one word. For every type the collector keeps a map of the words that may hold a pointer. The map starts full and is narrowed by elimination: a word that holds a non-zero value which is not an address in the managed heap is data, and its offset leaves the map for good. A pointer field only ever holds null or a managed address, so it never leaves. Three things follow:

- A `tracked_ptr` must not share its storage with data: no `union` of a pointer with a value, no `std::variant`, no `std::function` or `std::any` with inline storage holding a `tracked_ptr`. The same offset would be data in one object and a pointer in another, and the collector would stop following it. A union of two `tracked_ptr`s is fine, so is `std::optional<tracked_ptr<T>>`. Debug builds print a warning when they catch this.
- A raw `T*` to a managed object is not a reference the collector honours, inside a managed object as much as anywhere else: the object it points to lives as long as some `tracked_ptr` (or `unique_ptr`) keeps it, and using the raw pointer after that is undefined. That the collector's map may follow such a word in some cycle is an implementation detail, never a guarantee.
- A destructor must not read, dereference or copy the `tracked_ptr` members of the object it destroys, except through `if_alive()`. An object dies together with everything reachable only from it, in no particular order and on several threads, so a member may point at an object destroyed already; and the collector does not null such members beforehand, because its map is approximate and a store through it could hit data. `peer.if_alive()` is a copy of the pointer when the target is not dying in the same sweep and null when it is (outside a sweep it is always the copy: a live object's targets are live), so the way to reach a peer from a destructor is:

  ```cpp
  ~Node() {
      if (auto p = peer.if_alive()) {   // a copy when the peer lives, null when it dies in the same sweep
          p->detach(this);
      }
  }
  ```

  A live peer may be used and stored freely, a pointer to a dying one is never handed out. Owning `unique_ptr` members need no check: the owned object is alive until its owner destroys it, which is what `unique_ptr`'s destructor does. A container inside a dying object (a `list`, a `map`) leaves its nodes to the same sweep, which destroys the elements with them.

## Stack roots
An `sgcl::tracked_ptr` lives in one of two places: inside a managed object, where the collector finds it through the type's pointer map, or on a thread's stack, where the collector finds it by scanning the used part of the stack. Nowhere else: not in `new`/`malloc` memory, not in standard containers, not as a global or `thread_local` variable, not in a lambda that a `std::thread` or `std::function` copies to the heap. Debug builds assert this rule in the constructor; release builds trust it, and a violation leaves the object unprotected. A `gc::tracked_ptr` is what to use in those places: it puts the pointer into a managed cell of its own, and the cell is a root. To hold a pointer from outside the managed world, put it into a managed object created with `make_tracked` and keep that alive from a stack or a managed root.

The stack scan is conservative: every word of the stack that looks like a pointer into a live object counts as a root, and the mutator threads keep running meanwhile. An array (the buffer of a `vector` or `array`) is the exception: only a pointer to its first element counts, which is what its container holds; a word pointing at an element inside it is not a root, so a random word rarely retains a large buffer, and a pointer into a buffer whose container is gone is dangling, as in `std`. The cost of a `tracked_ptr` on the stack is therefore one word and a plain store; the only bookkeeping is a thread-local flag, so that a thread that has never touched the library gets its stack registered the first time it copies a pointer. Three consequences follow:

- A raw pointer, a reference or an iterator that stays in a frame keeps its target alive, even after the object logically died, until the word is overwritten. This is delay, not a leak: the frame ends, the object goes.
- `force_collect()` and the counting functions zero the unused stack below the caller's frame first (`config::StackClearSize`, 64 KB), so words left by dead frames do not distort a count taken right after a scope. What they cannot clear is the caller's own frame: a test that needs an exact count keeps the pointer-juggling code in a helper function.
- Fibers, signal stacks and stacks the library does not know about are not scanned. Thread stacks are registered on first contact and unregistered at exit, with a handshake so that a scan in progress finishes before the stack disappears. Only the pages a stack has actually used are read. When the used parts of all stacks add up to more than 4 MB, helper threads read them in pieces and hand the words that point into the heap to the collector thread, which marks them; below that the collector thread does it alone.

## Generations
An object that has survived one cycle is not traced again by the next ones: its mark bit stays set (sticky mark bits), and a young cycle traces only the objects marked for the first time, sweeping the unmarked ones. What the young cycle must still see is a pointer stored into an old object since the previous cycle. The write barrier records that on the page of the object holding the pointer, and the young cycle traces the old objects of those pages again, which costs one flag test per old child. Nothing moves and no object changes generation by copying: a young object that survives a cycle is old from then on.

The price is that garbage among the old objects waits for a full cycle. One runs after eight young cycles at the latest, sooner when the live memory has doubled since the last full cycle, under memory pressure, and for every `force_collect()` and the counting functions below, which therefore report complete collections. The knobs are `config::YoungCyclesMax` and `config::FullCycleGrowthPercent`.

The generations are on by default. `-DSGCL_GENERATIONAL=0` builds the collector with full cycles only and a barrier that does no carding, for a program that links objects more than it allocates them and holds little: the card costs 0.3 ns per store of a pointer into a heap object (a shift, a byte read and a compare; a copy into a `tracked_ptr` on the stack costs nothing extra, because the barrier recognises a location near its own frame without reading memory, which the guard chunks at both ends of the heap's range make sound). What the generations buy shows in the large-tree benchmark below, where a young cycle's cost does not grow with the old heap; against the build with full cycles only, the same run with a large live set and four threads allocating takes a quarter less CPU and a third less memory, at 5 to 8% of the mutators' speed in code that does nothing but link objects.

## Memory
The managed heap is one range of virtual address space reserved at start-up (several times the physical memory, placed at 1 TB where no short ASCII string on a stack reads as an address into it) and backed by the operating system lazily, in 2 MB chunks. Small objects live in pools of 64 KB pages whose chunks grow from the bottom of the range; buffers larger than a page take contiguous page ranges from the top, so the two never interleave and a freed buffer's range coalesces with its neighbours (free ranges are kept in bins by size). The reservation shows up as the process' virtual size (VIRT); the memory actually in use is the committed part, available as `sgcl::collector::get_committed_memory()`.

What the threads share is laid out by writer. The page header is one 128-byte line, the mutators' half (the fields a barrier reads, the flags a barrier or an allocation sets) in front of the collector's half (its lists and marks), and the slot states follow on lines of their own; the collector's mark words and the allocator's free bitmap are separate arrays; each thread's record, with the hazard pointer its atomic operations write, has a line of its own; and the heap's counters are pages, one relaxed `fetch_add` per page taken, on a line that nothing else touches. That last one is what the 24-thread allocation row above pays for: the four fenced updates it replaced cost a third of the time at that thread count.

Committed memory has a ceiling: by default 90% of the cgroup memory limit on Linux (containers, systemd slices), or of the physical memory elsewhere. Above 75% of the ceiling the collector cycles more often and returns free chunks to the system at once. When an allocation would cross the ceiling, it first forces a full collection; if that does not free enough, it throws `std::bad_alloc` instead of letting the process run into the OOM killer.

```cpp
// Current committed size of the managed heap in bytes
auto used = gc::collector::get_committed_memory();

// Ceiling on the committed size; 0 disables it
auto limit = gc::collector::get_memory_limit();
gc::collector::set_memory_limit(size_t(4) << 30);   // 4 GB
```
## Methods useful for state analysis
```cpp
// Forcing a collection: optional, the collector runs its cycles by itself
// (used in the examples and tests only to show or check a result at once)
gc::collector::force_collect();

// Forcing a collection and waiting for the cycle to complete
gc::collector::force_collect(true);

// Get number of live objects
// Note: A full GC cycle is performed before returning the data
auto live_object_count = gc::collector::get_live_object_count();
std::cout << "live object count: " << live_object_count << std::endl;

{
    // Get list of live objects
    // Note: A full GC cycle is performed before returning the data
    // Note: pause_guard and std::vector with raw pointers is returned
    //       The GC engine is paused until the pause guard is destroyed
    auto [pause_guard, live_objects] = gc::collector::get_live_objects();
    for (auto& v: live_objects) {
        std::cout << v << " ";
    }
    std::cout << std::endl;
} // The pause guard is destroyed at this point

// Zero the unused stack below the current frame: pointers left behind by
// dead frames stop keeping objects alive. Called automatically by
// force_collect() and the counting functions above; clear_stack(SIZE_MAX)
// zeroes down to the end of the thread's stack.
gc::collector::clear_stack();

// What the live heap is made of, by type, after a full cycle: objects by
// their type, the buffers of the containers by their array type (T[]),
// sorted by bytes. The answer to "what is growing".
for (auto& t : gc::collector::get_type_statistics()) {
    std::cout << (t.buffers ? "buffers of " : "") << t.type->name() << ": " << t.live_objects
              << " x " << t.object_size << " B = " << t.live_bytes << " B"
              << (t.buffers ? "" : ", " + std::to_string(t.pages) + " pages") << std::endl;
}

// Counters of the collector's work, without stopping it: cycles completed,
// live objects and memory after the last cycle, its duration and phases, the helpers
auto stats = gc::collector::get_statistics();
std::cout << stats.cycles << " cycles, " << stats.live_objects << " objects, "
          << stats.live_bytes / 1048576 << " MB, last cycle " << stats.last_cycle_ms << " ms, "
          << stats.last_helpers_used << " helpers" << std::endl;
for (int i = 0; i < 8; ++i) {                 // registration, states, roots, marking, updated, sweep, release, trim
    std::cout << gc::collector::phase_names[i] << " " << stats.phases_ms[i] << " ms" << std::endl;
}

// Stop the collector: cycles run until nothing dies any more, the threads exit.
// Optional; afterwards no cycle runs and tracked garbage stays until exit.
gc::collector::terminate();
```
## Benchmarks
What the tables measure is the management of pointers and nothing else: loops that do nothing but allocate, copy and drop, the worst case for any collector and not the part of a program C++ is chosen for. A C++ program with SGCL allocates a fraction of what the same program allocates in Java, because everything that can be a value is one; the tables ask a narrower question, what the part that does hold managed pointers costs next to the best runtimes. SGCL is a library without the compiler's help, and every gap to Go and Java below has that one source: a local `tracked_ptr` writes a byte of state on every copy because the library cannot know it never escapes, a store into an object the thread has just created stamps a card the compiler would elide, the stacks are scanned conservatively and an atomic load takes a hazard pointer where a stack map would do, and an allocation is a call into a per-thread bitmap where a JIT inlines a bump pointer and removes some allocations altogether. The gap is structural, a nanosecond or two per operation, and it ends there: from a few threads up, allocation and a shared graph are faster than in Go and Java, no mutator ever waits, and the memory sits at the level of Go's, which throttles its mutators to hold it, and below `shared_ptr`'s in every large case (binary-trees at depth 21: 368 MB against 516; the large tree of 16 million nodes: 815 MB against 2.0 GB). That last one is the argument C++ has for `shared_ptr` turned around: the memory is as bounded, at a third of the cost of a copy and without the destructor cascades. In exchange the library asks nothing of the compiler, the platform or the ABI.

The `benchmarks/` directory builds ten programs, each holding every C++ variant of one measurement (SGCL in both namespaces, `sgcl::` and `gc::`, then `unique_ptr`, `shared_ptr`, raw `new`/`delete` where it applies) and always compiled with `-O2 -DNDEBUG`; `benchmarks/run.sh <build-dir>` prints the C++ matrix. `benchmarks/go/` holds six of the programs in Go and `benchmarks/java/` in Java, the same shapes, and `benchmarks/compare.sh` runs every environment over several sizes of each problem, each run a process of its own under `/usr/bin/time` for its peak resident memory, and prints the best of three. Numbers below: a Mac Studio with an Apple M2 Ultra (16 performance and 8 efficiency cores, 64 GB), macOS 26.5, Apple clang 21.0 with `-O2`, Go 1.27.1 at its default `GOGC=100`, OpenJDK 26.0.2 with the generational ZGC (`-XX:+UseZGC`) under a heap ceiling of about twice SGCL's peak memory in the same case (`-Xmx256m` to `-Xmx4g`; with the default ceiling of a quarter of the machine ZGC hardly collects in runs this short, and the numbers would be those of an allocator, not a collector; Java's numbers move with the ceiling, more room meaning fewer collections and less CPU, and `compare.sh` takes another). Go paces its collector by a ratio to the live heap and Java by a ceiling, so the two are not given the same budget; both are run the way they are shipped. Best of 3 unless said otherwise, each run a fresh process. What the timers cover: the wall time starts right before the measured algorithm and stops when its threads have joined, so the collector's start (the heap reservation, its thread and helpers, the first cycles) is inside the window, as are Go's collector and Java's JIT warm-up; the JVM's own start-up is outside it, like the loading of any process. The CPU time is the whole process, the collector's threads after the algorithm included. The memory is the process's peak resident size.

The Boehm–Demers–Weiser collector, the one C++ has had for three decades, is not in the tables on purpose: it stops the world to find its roots and to mark, and its incremental mode buys shorter stops with page protection and a fault per first write. The runtimes SGCL is set against here are the concurrent ones, Go and Java's ZGC, whose mutators keep running while a cycle does its work; that is the class this library is in, and a comparison of allocation rates with a stop-the-world collector would say nothing about the pauses that separate the two.

The environments are not the same size: a Java object carries a 12-byte header and the graph's node holds its links in a separate array, Go's runtime scans its stacks precisely, and the JVM's numbers include its warm-up. The scripts are in the tree to rerun with other versions and sizes.

The two SGCL columns come from one run (`compare.sh` with `VARIANTS="sgcl gc"`, and `bench_containers` for each variant, the best of three), later than the run of the other columns, and the ratio in parentheses is `gc::` against `sgcl::` of that run. Between the runs the `sgcl::` numbers moved within their rounding, except for the memory-bound container cases, which move by a tenth from run to run (the `std` column of the container table is from the later run too, for that reason).

The current version has been tested on Apple Silicon only. The code has no dependency on the architecture beyond what the standard library and the system calls in `detail/os.h` provide, but no number below has been reproduced on x86-64 or on Linux and Windows yet.

### Allocation
Nanoseconds per object; each allocation retires the previous one, so the collectors have to keep up:

| size, threads | SGCL `sgcl::` | SGCL `gc::` | `unique_ptr` | `shared_ptr` | Go | Java ZGC |
|---|---|---|---|---|---|---|
| 32 B, 1 | 5.4 | 5.3 (0.98×) | 21.7 | 21.9 | 7.0 | 3.1 |
| 32 B, 4 | 5.9 | 6.0 (1.01×) | 37.5 | 37.7 | 32.9 | 6.9 |
| 32 B, 24 | 12.3 | 12.2 (0.99×) | 90.7 | 107.2 | 291.1 | 26.9 |
| 256 B, 1 | 6.5 | 6.6 (1.01×) | 21.0 | 23.7 | 97.7 | 9.0 |
| 256 B, 4 | 9.3 | 9.5 (1.02×) | 36.1 | 47.1 | 284.1 | 25.3 |
| 256 B, 24 | 48.9 | 49.0 (1.00×) | 82.3 | 113.5 | 2182.2 | 97.1 |

On one thread Java's bump allocation in a thread-local buffer is the fastest (3.1 ns for 32 bytes against SGCL's 5.4); from four threads up SGCL is (5.9 ns against Java's 6.9 and Go's 33 at four, 12 against 27 and 291 at 24), because its mutators never wait for the collector and its per-thread page allocator hands out slots without a lock or a barrier. Go's allocator pays for 256-byte objects with its size classes and assists (98 ns on one thread, 2.2 µs on 24), Java's ZGC with its allocation barriers and, under its ceiling, the collections it has to run (27 ns at 24 threads, 97 ns for 256-byte objects), `unique_ptr` and `shared_ptr` with malloc (21 to 22 ns on one thread, 82 to 113 on 24) and the second with its control block. The `gc::` pointer that keeps the newest object here is a local, an `sgcl::tracked_ptr` word once its constructor has checked the address: the same cost.

### Pointer copy
Nanoseconds per copy of a pointer to a live object, 50 million per thread: for SGCL the write barrier, for `shared_ptr` the reference count, for Go and Java the store with their barriers. `unique_ptr` has no copy: the column is the raw pointer a program built on `unique_ptr` hands around instead, the plain store every other column adds its bookkeeping to. "local" is a store into a variable on the stack, "field" into a member of a heap object; with four threads every thread copies pointers to the same object.

| threads, store | SGCL `sgcl::` | SGCL `gc::` | `unique_ptr` (raw pointer) | `shared_ptr` | Go | Java ZGC |
|---|---|---|---|---|---|---|
| 1, local | 1.4 | 1.5 (1.08×) | 0.6 | 4.7 | 0.6 | 0.9 |
| 1, field | 1.8 | 1.9 (1.05×) | 0.6 | 4.7 | 0.7 | 1.0 |
| 4, local, shared target | 1.4 | 1.5 (1.10×) | 0.6 | 127.0 | 0.6 | 0.9 |
| 4, field, shared target | 1.9 | 1.9 (1.05×) | 0.6 | 254.3 | 0.7 | 1.1 |

The raw pointer is the floor, 0.6 ns for the load and the store. SGCL adds a byte of state written on every copy (1.4 ns), and half a nanosecond more into a field for the card ("Generations" above). Go's write barrier is a flag test while no cycle is marking, and nothing allocates in this loop, so none is; a local costs Go nothing beyond the store, as its stacks are scanned precisely. Java pays ZGC's load barrier on the read of the pointer and its store barrier on the field. `shared_ptr` pays two atomic count updates, and a contended cache line when threads share the object: a hundred times SGCL's cost. A `gc::` store adds the test of the mode, 0.1 ns; it is the construction of a `gc::tracked_ptr` that costs, the check of its address (the range of the heap, then the thread's stack bounds from a thread-local), 0.7 ns on the stack and a cell taken from the thread's block in unmanaged memory.

### Lock-free stack
A Treiber stack (the shape of `examples/lock_free_stack.cpp`): a compare-exchange on the head, with a collector free of ABA because a node is never reused while a thread holds it. Go's variant uses `atomic.Pointer`, Java's `AtomicReference`. The `shared_ptr` variant uses the standard library's atomic operations on a `shared_ptr` (`std::atomic_load`, `std::atomic_compare_exchange_weak`), which it implements with a lock; the `unique_ptr` variant, where every node has one owner, is a stack under a mutex, the classic answer without a collector. Every thread pushes a node and pops one, a million times; nanoseconds per push or pop:

| threads | SGCL `sgcl::` | SGCL `gc::` | `unique_ptr` | `shared_ptr` | Go | Java ZGC |
|---|---|---|---|---|---|---|
| 1 | 13.0 | 19.0 (1.46×) | 18.6 | 39.1 | 11.3 | 13.1 |
| 4 | 83.9 | 96.6 (1.15×) | 54.8 | 139.1 | 54.6 | 60.1 |
| 16 | 586.2 | 582.0 (0.99×) | 35.7 | 88.6 | 656.3 | 653.8 |

The three collectors are within a few nanoseconds of each other on one thread. At four threads Go is the fastest of them (55 ns against Java's 60 and SGCL's 84): an operation costs SGCL a few `tracked_ptr` temporaries (the loaded head, the arguments of the compare-exchange, taken by value so that the target is held for the length of the call, as `std::atomic<shared_ptr>` does), each a write barrier with its card, and a hazard pointer on every load, and the cards on the stores into the shared nodes add to the retries. At sixteen threads every compare-exchange variant drowns in retries and the locks win, the mutex of the `unique_ptr` variant first; that is the algorithm, not the collector. In a second mode, "pairs" (half the threads push a million nodes each, the other half pop them), SGCL takes 60, 138 and 658 ns at two, four and sixteen threads, and the `shared_ptr` variant does not survive sixteen: a consumer descheduled while holding a popped node keeps every node popped after it alive through the `next` links, and releases the whole chain at once, recursively, off the end of its stack. A collector has no such chain to release. The `gc::` stack (its head a `gc::atomic`, its nodes linked by `gc::tracked_ptr`) pays the location check on the `gc::tracked_ptr`s the operation builds, the loaded head and the new node; the arguments of the compare-exchange are `sgcl::tracked_ptr`s inside the atomic, copied from the word a `gc::tracked_ptr` holds without a check: 6 ns on one thread, less in proportion once the retries dominate.

### binary-trees
The benchmarks-game program: a long-lived tree of the maximum depth held for the whole run while trees of every smaller depth are built and dropped, on one thread or on four in parallel. Wall time / process CPU time in seconds / peak resident memory; for SGCL the destruction runs on the collector's thread, so it moves from the wall time into the CPU time:

| depth, threads | SGCL `sgcl::` | SGCL `gc::` | `unique_ptr` | `shared_ptr` | Go | Java ZGC |
|---|---|---|---|---|---|---|
| 16, 1 | 0.13 / 0.23 / 23 MB | 0.22 / 0.32 / 20 MB (1.69× / 1.39×) | 0.37 / 0.38 / 6 MB | 0.40 / 0.41 / 18 MB | 0.16 / 0.39 / 17 MB | 0.08 / 0.20 / 229 MB |
| 16, 4 | 0.04 / 0.22 / 57 MB | 0.07 / 0.31 / 53 MB (1.75× / 1.41×) | 0.21 / 0.73 / 6 MB | 0.23 / 0.79 / 26 MB | 0.09 / 0.51 / 22 MB | 0.05 / 0.27 / 231 MB |
| 18, 1 | 0.58 / 1.11 / 84 MB | 1.02 / 1.56 / 69 MB (1.76× / 1.41×) | 1.69 / 1.69 / 18 MB | 1.88 / 1.88 / 66 MB | 0.67 / 2.13 / 44 MB | 0.29 / 0.60 / 512 MB |
| 18, 4 | 0.17 / 1.02 / 256 MB | 0.29 / 1.53 / 185 MB (1.71× / 1.50×) | 0.84 / 3.03 / 22 MB | 0.85 / 2.95 / 108 MB | 0.35 / 2.66 / 51 MB | 0.16 / 1.02 / 578 MB |
| 21, 1 | 5.33 / 12.64 / 368 MB | 9.29 / 16.74 / 282 MB (1.74× / 1.32×) | 15.42 / 15.40 / 130 MB | 17.00 / 16.98 / 516 MB | 5.86 / 23.16 / 220 MB | 2.63 / 7.51 / 1.1 GB |
| 21, 4 | 1.91 / 11.33 / 399 MB | 3.25 / 16.13 / 302 MB (1.70× / 1.42×) | 7.24 / 24.59 / 130 MB | 8.81 / 29.24 / 516 MB | 3.19 / 26.59 / 214 MB | 1.88 / 10.26 / 1.1 GB |

Java is the fastest in wall time (2.6 s at depth 21 on one thread against SGCL's 5.3, 1.88 s on four against 1.91) and the cheapest in CPU (7.5 and 10.3 s against SGCL's 12.6 and 11.3); SGCL's CPU is the young cycles at work over the four million nodes of the long-lived tree, which they leave alone, and the sweep of everything else. Go's loop is close to SGCL's in wall time and pays twice the CPU (23 s on one thread), its assists throttling the mutator to what the collector traces. Memory: `unique_ptr` holds only the live trees (130 MB), SGCL 368 and 399 MB (the garbage among the old objects waits for a full cycle), Go 214 and 220 MB, `shared_ptr` 516 MB (malloc keeps what the cascades free), Java at its ceiling, 1.1 GB. At depth 16 the runs are too short to mean much (the Java ones include the JIT's warm-up). The `gc::` tree is the case the family pays most for: a node is two `gc::tracked_ptr` members, each constructed with the location check, linked through two stores that test the mode, and read through the test of the sign, and the program does nothing else; 1.7 times the wall time of `sgcl::`, still faster than `unique_ptr`'s frees.

The price of SGCL's wall time is CPU: at depth 18 the program allocates 113 million objects per second on one thread, and keeping up takes the collector's thread plus helpers. A cycle starts once the pages allocated since the last one reach a quarter of what it left in use (4 MB at least), so the garbage waiting for a sweep is bounded by the live heap, not by the collector's speed. The passes over pages are shared with a pool of helpers only while the collector is behind (the memory in use grew by 16 MB and the mutators keep allocating), and marking joins them once a cycle marks a million objects (`config::MarkObjectThreshold`): at depth 18 one mutator marks on one thread and peaks at 84 MB, four mutators push the cycle onto the pool and peak at 232 MB.

Marking on the pool. Every thread traces from a stack of its own: an object whose mark bit it sets (one relaxed `fetch_or`, after a plain test that spares the objects marked already) goes on the stack, and half of that stack, the older half, goes to a shared pile whenever another thread is parked for want of work. The pass ends when every thread is parked. `bench_marking` builds a binary tree of 2 million or 8 million nodes and forces cycles on the stable heap; the marking pass alone, in milliseconds, by the number of helpers (a build with `-DSGCL_MARK_STATS` reports it, see the file):

| live objects | one thread, page order | 1 helper | 2 helpers | 4 helpers | 8 helpers |
|---|---|---|---|---|---|
| 2 M | 15.8 (7.6 ns/object) | 12.0 | 7.7 | 5.2 | 3.4 (1.6 ns/object) |
| 8 M | 66.3 (8.0 ns/object) | 48.0 | 30.8 | 19.5 | 11.4 (1.4 ns/object) |

The parallel code on one thread alone marks at 11 ns per object, the depth-first stack against the page-ordered sweep of the mark bits, and every helper adds its share of CPU: at eight helpers the pass costs 1.7 times the CPU of the sequential one. What it buys is a shorter cycle, which matters when the collector is behind. binary-trees at depth 21 keeps a tree of 4 million nodes alive, and its one mutator allocates faster than a 50 ms cycle can sweep: the collector runs cycle after cycle, 104 of them in the 4.8 s of the run, and the garbage of the cycle in progress peaks at 360 MB of resident memory. With marking on the pool the cycle takes 13.5 ms, the collector runs 345 of them, the peak drops to 222 MB, and the process CPU time rises from 18.8 s to 35.6 s. The wall time is the same (4.9 s), because no mutator ever waits for the collector. With four mutators at that depth the peak goes from 6.3 GB to 4.1 GB for 14.3 s against 18.9 s of CPU. The threshold keeps the smaller heaps, where a cycle is short anyway, on one thread.

### A shared graph
Latency of single operations on a graph shared by 16 threads for 3 seconds, nanoseconds (the clock ticks every 42 ns on this machine). "insert" allocates a node linked to four random nodes and replaces a random root of the thread's own (4096 or 65536 roots per thread, the size of the live set); "walk" follows 32 random links copying the pointer at every step; "drop-all" clears every root at the end and times the longest clear. The `unique_ptr` variant does not exist: a node of this graph has as many owners as links to it.

| roots per thread | variant | insert p50 / p99 / p99.9 | walk p50 / p99 / p99.9 | drop-all | ops/s | RSS |
|---|---|---|---|---|---|---|
| 4096 | SGCL `sgcl::` | 42 / 333 / 3000 | 1125 / 2875 / 5917 | 6 µs | 17.8 M | 1.4 GB |
| 4096 | SGCL `gc::` | 42 / 334 / 3083 | 1166 / 2834 / 5916 | 5 µs | 17.7 M | 1.6 GB |
| 4096 | `shared_ptr` | 125 / 709 / 3541 | 1625 / 2875 / 7334 | 69.7 ms | 12.9 M | 1.3 GB |
| 4096 | Go | 42 / 1167 / 4916 | 1083 / 2375 / 5417 | 5 µs | 17.7 M | 1.9 GB |
| 4096 | Java ZGC | 333 / 3292 / 6667 | 1500 / 4542 / 9875 | 111 µs | 11.8 M | 1.5 GB |
| 65536 | SGCL `sgcl::` | 84 / 625 / 3209 | 834 / 2667 / 4500 | 22 µs | 20.9 M | 1.7 GB |
| 65536 | SGCL `gc::` | 84 / 667 / 3291 | 833 / 2667 / 4458 | 25 µs | 20.9 M | 1.8 GB |
| 65536 | `shared_ptr` | 292 / 2416 / 4250 | 834 / 2667 / 4917 | 166.1 ms | 18.5 M | 2.1 GB |
| 65536 | Go | 42 / 1208 / 5417 | 584 / 2000 / 3500 | 23 µs | 16.2 M | 1.6 GB |
| 65536 | Java ZGC | 459 / 3416 / 7375 | 1083 / 3709 / 24250 | 17.0 ms | 14.2 M | 2.0 GB |

The medians are close; the tails and the throughput tell the story. With the live set large and shared, SGCL has the highest throughput (17.8 to 20.9 million operations per second against Go's 16.2 to 17.7 and Java's 11.8 to 14.2), the lowest insert latency at the median and p99 (42 and 333 ns with 4096 roots against Go's 42 and 1167), and Go the lowest walk latency with 65536 roots (584 ns against 834). Java's insert is two allocations (a node and its array of links) with ZGC's barriers on every reference load, which is what its p50 of 333 ns is, and its walk tail (10 to 24 µs at p99.9) is where the mutators wait for the collector under the ceiling; its numbers time one insert and one walk in eight, since `System.nanoTime()` on macOS serializes the threads when called twice per operation. `shared_ptr` pays the destructor cascade in the mutator thread the moment a large graph is dropped: 70 and 166 ms for a drop-all that costs the collectors microseconds. What a collector does cost is cores: SGCL's thread and, on this graph, its marking helpers compete with sixteen mutators for sixteen performance cores, which is where its p99.9 comes from; the CPU time of every variant is close to the 48 to 65 s of sixteen threads for three seconds. Memory is even, 1.3 to 2.1 GB across the board. On this graph the two families are within the run-to-run spread of each other: the walk is a chain of loads whose pointer test is hidden under the cache misses, and an insert is an allocation.

### Containers
`benchmarks/containers.sh` runs each container case in a process of its own against the `std` counterpart: one million elements, nanoseconds per operation, the footprint of the built container and the peak resident size of the run. The footprint is what the container occupies once the garbage of building it is gone: for SGCL the pages holding live managed objects after a full collection (free slots left in a page by erasures stay counted, they are reusable by objects of that type), for `std` the bytes malloc reports in use (blocks freed and kept by malloc are not counted). The peak RSS adds what waits for a collection on one side and what malloc keeps on the other.

The node containers cost what `std`'s do or less: allocation is the collector's, iteration is a plain load per step, a node is as big as its `std` counterpart and pays no malloc rounding (a 24-byte list node takes 32 bytes from malloc). A `push_back` compares two words of the vector object, stores the element and stores the count; the count is reloaded on the next push (a store the compiler cannot keep in a register across the growth call), which `std::vector` avoids with its end pointer, and a fresh buffer is fresh pages, faulted in on first touch, where `malloc` hands back memory it already touched. The maps pay for the write barriers on the links they relink. The `gc::` column pays the test of the mode on the container's root word and nothing per element: a `gc::vector<gc::tracked_ptr<T>>` stores `sgcl::tracked_ptr`s (an element type that names a `tracked_type` is stored as that type, one word in the same mode inside a managed buffer or node) and hands them out as `gc::tracked_ptr<T>&`.

| case | SGCL `sgcl::` ns | SGCL `gc::` ns | `std` ns | SGCL footprint | `std` footprint | SGCL peak | `std` peak |
|---|---|---|---|---|---|---|---|
| vector push_back | 2.7 | 2.7 (1.00×) | 1.6 | 8.8 MB | 8.0 MB | 18 MB | 17 MB |
| vector of pointers, copy and walk | 2.9 | 3.2 (1.10×) | 9.2 (`shared_ptr`) | 16.4 MB | 31.3 MB | 27 MB | 64 MB |
| deque push both ends | 1.8 | 1.9 (1.06×) | 1.4 | 8.9 MB | 7.7 MB | 11 MB | 9 MB |
| list push_back / iterate | 18.0 / 2.0 | 18.5 / 2.0 (1.03× / 1.00×) | 25.9 / 2.0 | 23.0 MB | 30.5 MB | 27 MB | 32 MB |
| list erase every other | 20.0 | 20.4 (1.02×) | 25.9 | 23.0 MB | 15.3 MB | 27 MB | 32 MB |
| forward_list push_front | 14.0 | 14.2 (1.01×) | 21.7 | 15.4 MB | 15.3 MB | 19 MB | 17 MB |
| map insert / find / iterate | 366 / 297 / 134 | 351 / 277 / 144 (0.96× / 0.93× / 1.08×) | 398 / 300 / 146 | 45.9 MB | 45.8 MB | 57 MB | 55 MB |
| set insert | 307 | 354 (1.15×) | 360 | 38.2 MB | 45.8 MB | 50 MB | 55 MB |
| unordered_map insert / find | 187 / 244 | 179 / 220 (0.96× / 0.90×) | 205 / 260 | 39.3 MB | 43.1 MB | 58 MB | 65 MB |
| unordered_map erase half | 381 | 410 (1.08×) | 427 | 39.3 MB | 27.8 MB | 58 MB | 65 MB |
| unordered_map of pointers | 108 | 93 (0.86×) | 156 (`shared_ptr`) | 47.0 MB | 88.9 MB | 68 MB | 111 MB |

### A large live tree
The case a tracing collector likes least: a tree of 4 or 16 million nodes (depth 22 or 24, 128 or 512 MB of nodes) held for the whole run while one or four threads make and drop 500,000 small trees each (depth 8, 511 nodes, 255 million allocations per thread). Every full cycle has to trace the large tree again; a young cycle ("Generations" above) traces the young objects and the old pages written since the last cycle instead. Java ZGC under `-Xmx1g` for the 4 million nodes and `-Xmx4g` for the 16 million. Wall time of the loop / process CPU time / peak resident memory:

| nodes, threads | SGCL `sgcl::` | SGCL `gc::` | `unique_ptr` | `shared_ptr` | Go | Java ZGC |
|---|---|---|---|---|---|---|
| 4 M, 1 | 2.19 s / 5.1 s / 214 MB | 3.84 s / 6.8 s / 205 MB (1.75× / 1.34×) | 6.28 s / 6.4 s / 130 MB | 6.69 s / 6.8 s / 516 MB | 2.42 s / 10.3 s / 284 MB | 1.45 s / 4.6 s / 1.1 GB |
| 4 M, 4 | 2.31 s / 18.7 s / 357 MB | 3.92 s / 25.5 s / 287 MB (1.70× / 1.36×) | 8.09 s / 32.2 s / 131 MB | 12.56 s / 50.3 s / 516 MB | 4.61 s / 45.5 s / 307 MB | 1.67 s / 7.9 s / 1.1 GB |
| 16 M, 1 | 2.24 s / 6.0 s / 815 MB | 3.87 s / 7.9 s / 775 MB (1.73× / 1.31×) | 6.30 s / 6.8 s / 516 MB | 6.70 s / 7.3 s / 2.0 GB | 2.50 s / 11.9 s / 1.1 GB | 2.42 s / 11.9 s / 4.1 GB |
| 16 M, 4 | 2.38 s / 19.5 s / 1.1 GB | 3.98 s / 26.7 s / 978 MB (1.67× / 1.37×) | 8.13 s / 32.6 s / 516 MB | 12.04 s / 48.7 s / 2.0 GB | 4.72 s / 48.5 s / 1.1 GB | 3.74 s / 19.2 s / 4.1 GB |

- **The mutators do not feel the live set.** SGCL's loop takes the same 2.2 to 2.4 s over 4 million nodes as over 16 million, on one thread and on four (865 thousand small trees per second on four threads, 3.8 times one thread): no thread ever waits for a cycle, and a cycle only takes longer on the collector's cores. The loop took 110 young cycles and 12 full ones over the 4 million nodes on one thread, 308 and 34 on four; 29 and 4, 87 and 10 over the 16 million. Go's loop is 10% slower on one thread and twice as slow on four, where the assists throttle the mutators to what the collector can trace (46 to 48 s of CPU for 4.6 s of wall). Java is faster than SGCL over the 4 million nodes (1.45 s on one thread, 1.67 s on four) and slower over the 16 million (2.4 and 3.7 s): at that size its collector no longer keeps up under the ceiling and the allocation stalls of ZGC show in the wall time, while its memory sits at the ceiling (1.1 and 4.1 GB).
- **The young cycles leave the tree alone.** A full cycle traces the whole tree; a young cycle registers and sweeps what was allocated since the last one and traces the survivors and the cards, so its cost does not grow with the old heap: 5.1 s of CPU for the loop over 4 million nodes and 6.0 s over 16 million on one thread, 18.7 and 19.5 s on four. The mutators pay 6 to 8% of their speed for the card stamped by every store of a pointer into a heap object, and building a tree is nothing but such stores.
- **Without a collector the mutator pays the frees.** `unique_ptr` and `shared_ptr` free each small tree on the thread that made it, 511 frees in a recursive destructor per tree, and through malloc's contention on four threads: 6.3 and 6.7 s of wall on one thread, 8.1 and 12.6 s on four, three to five times SGCL's. `unique_ptr` holds the least memory of the table, since nothing waits for a cycle; `shared_ptr` holds the most of the C++ variants (516 MB and 2.0 GB against SGCL's 214 MB and 815 MB on one thread): a node of two `shared_ptr`s with its control block is three times SGCL's node of two words, and malloc keeps what the cascades free.
- **The `gc::` tree is the binary-trees case again**: two `gc::tracked_ptr` members per node, each constructed with the location check and linked with a store that tests the mode, 1.8 times the wall time of `sgcl::` and still a third of `unique_ptr`'s; the memory is the same, and the mutators still do not feel the live set (4.0 s over 4 million nodes, 4.0 over 16 million).

## Dependencies and usage
C++20 and nothing else: no external library, no runtime to link. Copy the `sgcl` and `gc` directories into your include path and `#include "sgcl/sgcl.h"` (both namespaces; `"gc/gc.h"` is the same), or add this tree with CMake and link the `sgcl` interface target. The tests need googletest in `external/`; the benchmarks build with the tree, and their Go and Java counterparts need only a Go and a JDK to run `benchmarks/compare.sh`.

## Compilers and platforms
Written for clang, gcc and MSVC on macOS, Linux and Windows; the current version has been built and tested on Apple Silicon (macOS, Apple clang) only, the other platforms are pending. On Windows, gcc's handling of thread-local destructors makes it a poor choice; clang and MSVC are fine. On macOS every access to a thread-local variable is a call into the dynamic loader, which is what the registration check in a `tracked_ptr` constructor costs there (about a nanosecond); Linux and Windows read a segment register.

## License
Apache License 2.0, see [LICENSE](LICENSE). Contributions are accepted under the same terms (section 5 of the license), with no separate agreement.
