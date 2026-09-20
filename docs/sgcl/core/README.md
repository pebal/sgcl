# sgcl::core

The collector and the pointers: what every other module is built on and what a program needs before anything else. `#include "sgcl/core/core.h"` brings the module in; the module depends on nothing but the standard library.; the index of the whole interface is [`docs/sgcl/`](../README.md).

What the module holds: the pointer the collector follows and the two pointers around it (the one `make_tracked` returns and the one that does not keep its object alive), a root that may live in unmanaged memory, the creation of objects on the managed heap, the collector's own interface (a forced cycle, statistics, the memory limit), the compile-time configuration, the value types that keep a tracked pointer apart from data (`variant`, `any`, `function`, `expected`), `string` (immutable, one word, shared by copying; split, join, trim, replace; `parse` and `to_string`) and `slice` (a piece of a string, or of any contiguous managed storage, that holds the object; a span when the memory is unmanaged), `range` (a pair of iterators as a range, and the integers of `range(n)`), the `std` types under the library's names, and the mixins and the requirements ([mixin/](mixin/README.md), [req](req.md)): the bases every container of the library declares itself by — `mixin::enumerable`, `mixin::ordered`, `mixin::sequence`, `mixin::lookup`... — which give it `contains`, `sort`, `get` as members, and the requirements `req::enumerable`, `req::ordered`, `req::lookup`... a function's parameter asks for. The engine under it (the heap, the barrier, the roots, a cycle phase by phase), its diagnostics and its constants have a chapter of their own, [the garbage collector](../../garbage_collector/README.md); the code is in `sgcl/core/detail/`.

## The pointer and its places
`sgcl/sgcl.h` declares the namespace `sgcl`. The word by which every type holds its memory is `tracked_ptr<T>`: the pointer the collector follows, one word, a store and a byte of state per copy. The collector finds such words in two places only, inside managed objects and on the stacks it scans, so a `tracked_ptr`, and every container, atomic, weak pointer or coroutine handle, lives there and nowhere else: never in `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a lambda copied to the heap (rule 1 below). Debug builds assert it.

For those places there is `root_ptr<T>`: a root that lives anywhere, over a cell, a word of a managed block of a cache line of them that is the object's root, taken by the constructor from the thread's allocator, given back by the destructor, the `root_ptr`'s own in between. The cell is a `tracked_ptr` inside a managed object, so the `root_ptr` reads and writes through it, with its barrier, and hands it out by reference (`ptr()`) for the code that lives where a `tracked_ptr` may; no store allocates and no move takes a cell from another `root_ptr`, so threads race on the cell's word exactly as on a `tracked_ptr`, never on the making of a cell; a block is one managed allocation per sixteen cells and is freed by the collector once every cell of it is given back. A container that must live in such a place goes into a managed object under a `root_ptr` ([docs/sgcl/root_ptr.md](root_ptr.md)).

## The classes

- `unique_ptr<T>`: what `make_tracked<T>(...)` returns. A specialization of `std::unique_ptr` whose object lives on the managed heap: destroyed at scope exit like any `unique_ptr`, and the root of whatever it owns meanwhile; lives anywhere. Converts into a `tracked_ptr`, after which the object belongs to the collector.
- `tracked_ptr<T>`: the pointer the collector follows. Copies, converts to base classes, compares, `reset()` and `reset(T*)`, `get()`; `type()`, `is<U>()` and `as<U>()` for the dynamic type of the object; `if_alive()` for the one situation a pointer may be dangling, a destructor reading a peer that may be dying in the same sweep ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)); `to_shared()` is a `std::shared_ptr` that holds the object from unmanaged memory, through a managed holder its control block owns ([The rules](README.md#the-rules)).
- `root_ptr<T>`: a root that lives anywhere (a global, a `std::vector`, a handle table, a lambda on the heap): a cell of a managed block under it, the object reachable while the `root_ptr` exists; `ptr()` is the `tracked_ptr` it holds its object by, for the code that lives where one may, and for an `atomic_ref` over the root.
- `atomic<tracked_ptr<T>>` and `atomic_ref<tracked_ptr<T>>`: `load`, `store`, `compare_exchange_weak` and `compare_exchange_strong` with `std::memory_order`, plus `wait`/`notify`; lock-free, the loaded object protected by a hazard pointer for the length of the load. A global shared pointer is a `root_ptr` under an `atomic_ref`. `atomic<string>` is the same over the word of a `string`: a string variable that one thread replaces while others read it, one word, no copy. For any other type `atomic<T>` is `std::atomic<T>`, so that a program names one atomic for its flags, counters and pointers.
- `managed_frame`, `frame_ptr<Promise>`, `task<T>`, `generator<T>`, `scheduler`, `spawn`, `yield`: coroutines with frames on the managed heap, and the pool of workers that runs the tasks ([Coroutines](../async/README.md#coroutines)).
- `expiry_queue<T>`: `watch(object, f)` is an entry, `f` kept for the day nothing else reaches the object (its handle cancels the entry, or gives a `weak_ptr`); `drain()` calls the `f` of every such entry with the object, alive one last time ([Weak pointers](README.md#weak-pointers)).
- `weak_ptr<T>`: a pointer that keeps nothing alive. `lock()` is the object as a `tracked_ptr` while it is reachable and null once a cycle has found it unreachable; `expired()`, `reset()`; made from a `tracked_ptr` or another `weak_ptr` ([Weak pointers](README.md#weak-pointers)).
- `weak_map<Key, T>`, `weak_multimap<Key, T>`, `weak_set<Key>`: containers keyed by objects they do not keep alive; an entry dies with its object ([Weak containers](../containers/README.md#weak-containers)).
- `variant<Ts...>`, `any`, `function<R(Args...)>`, `move_only_function`, `expected<T, E>`: the interfaces of their `std` namesakes, safe to hold a `tracked_ptr` or a `weak_ptr` next to other alternatives, values, captures or errors, which the `std` ones are not ([variant, any, function and expected](README.md#variant-any-function-and-expected)). `optional`, `pair` and `tuple` hold one correctly as they are and are aliased under the library's names, so that the safe set is one namespace.
- `string` (`basic_string<CharT>`, `wstring`, `u8string`, `u16string`, `u32string`): an immutable string on the managed heap, one word, shared by copying, compared and hashed by its contents, no destructor ([string](#string)).
- `concurrent_queue<T>`, `concurrent_stack<T>`, `concurrent_sorted_map<Key, T>`, `concurrent_sorted_set<Key>`, `concurrent_map<Key, T>`, `concurrent_set<Key>`: lock-free structures shared by any number of threads, with `push`/`try_pop`/`pop`, and `find`/`insert`/`try_emplace`/`erase` with weakly consistent iteration for the maps and sets; `copy_on_write<T>`: a value loaded as an immutable snapshot and replaced whole by a copy and a compare-exchange; `channel<T>`: `send`/`receive` with the waiting of both sides, `try_*`, `async_*` for coroutines, `close` ([Lock-free containers](../concurrent/README.md#lock-free-containers)).
- The containers, listed below.

## make_tracked
`sgcl::make_tracked<T>(args...)` creates an object on the managed heap and returns a `unique_ptr<T>`: deterministic until it is converted into a `tracked_ptr` or dropped. Managed arrays are not a public type: `sgcl::vector` and `sgcl::dynamic_array<T>` own their buffers on the managed heap.

## Pointer aliases
A `tracked_ptr` may point into the middle of a managed object: to a member or to a base subobject. Such an alias behaves like the aliasing constructor of `std::shared_ptr`: the object it points into stays alive for as long as the alias does.

```cpp
struct Item { int value; string name; };
tracked_ptr item = make_tracked<Item>();
tracked_ptr<int> alias(&item->value);
item = nullptr;                    // the Item lives on: the alias keeps it
```

What a `tracked_ptr` may not address is an element of a container's buffer (`sgcl::vector`, `sgcl::dynamic_array<T>`): a buffer is rooted only through the pointer to its first element that the container holds, an alias into it would keep nothing, and debug builds assert on the attempt. A pointer or reference to an element of any `sgcl` container is exactly as valid as with its `std` counterpart: until the element is removed, the container reallocates, or the container is destroyed.

## variant, any, function and expected
`std::variant` keeps every alternative at the same offset and `std::any` keeps a small value in a buffer inside itself: a `tracked_ptr` there shares its word with the data of the other alternatives or values, the collector's pointer map, built by elimination, finds data at that offset in some object and drops the offset for good, and the pointer is no longer followed ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)). `sgcl::variant<Ts...>` and `sgcl::any` have the same interfaces (`get`, `get_if`, `holds_alternative`, `visit`, `emplace`, `any_cast`, `make_any`, the comparisons, `std::hash`, the exceptions of `std`) and lay their contents out by what they hold: a pointer word (`tracked_ptr` of either kind, `weak_ptr`) goes into a word that holds null or an address and nothing else, the alternatives of a `variant` that may hold pointers among their data get places of their own, and the alternatives and values that cannot hold a pointer share the data storage. An `any` holds a small pointer-free value (16 bytes) inline and anything else, a container with pointers included, in a managed node of its own, held by a pointer in the same word and traced through its own pointer map, so that a value pointing back at the `any`'s owner is a cycle collected like any other; the value is destroyed the moment the `any` drops it, as a container destroys an erased element, and the node is reclaimed later. `function<R(Args...)>` and `move_only_function` (with the signature's `const` and `noexcept`) keep a closure the same way: a small one without pointers inline, one capturing tracked pointers in a node of its own, which is what lets a callback, an observer or an `expiry_queue` entry capture the objects it works on, a closure capturing its own owner included; 32 bytes, like `std::function`. `expected<T, E>` (the C++23 interface, with `unexpected`, `unexpect`, `bad_expected_access` and the monadic operations) is a `variant` of the value and the error. `any` and `function` take the kind of their word as their last parameter, like the containers: `sgcl::any` and `sgcl::function` live where a `tracked_ptr` may, `sgcl::any` and `sgcl::function` anywhere. `variant` and `expected` have no word of their own, so where one may live is decided by what it holds: with `sgcl::tracked_ptr`s inside, where a `tracked_ptr` may; with `sgcl::tracked_ptr`s inside, anywhere; `sgcl::variant` and `sgcl::expected` are the same types. The `variant` and the `expected` are not `constexpr`, and the ones of pointer-free alternatives are what the `std` types are for. `optional`, `pair` and `tuple` keep a pointer at a fixed offset of its own and are safe as they are: `sgcl::optional`, `sgcl::pair`, `sgcl::tuple` (and under `sgcl::`) name the `std` types, so that the safe set is one namespace (`sgcl/core/aliases.h`).

```cpp
struct Node { int value; };
variant<int, tracked_ptr<Node>, std::string> v = make_tracked<Node>(1);   // the pointer in a word of its own
v = 5;                                                   // the int elsewhere: the word is null now
any a = vector<tracked_ptr<Node>>{make_tracked<Node>(2)};   // the vector in a managed node of its own
if (auto p = any_cast<vector<tracked_ptr<Node>>>(&a)) {
    std::cout << (*p)[0]->value << "\n";
}
tracked_ptr node = make_tracked<Node>(3);
function<int()> f = [node] { return node->value; };   // the closure in a managed node of its own: node lives while f holds it
expected<tracked_ptr<Node>, std::string> r = unexpected("not found");   // the pointer and the string laid out apart
std::cout << f() << " " << r.error() << "\n";
```

## string
`sgcl::string` is an immutable string on the managed heap: one word, a pointer to an object holding the length, the characters and a terminator, and the hash once something has asked for it, of exactly that size (a string of ten characters is an object of 20 bytes). What a string is in Java or Go rather than in C++: made once, never modified, shared by copying the word, compared and hashed by its contents, reclaimed by the collector, with no destructor and no reference count. The empty string is null. There is no small-string optimization, and the string is not a buffer to build in: text is built as a `std::string` or a `string_view` and made a `string` once; the read side of `std::string` and all of `std::string_view` are there (`size`, `[]`, the `find`s, `starts_with`, `substr` as a new string, `+` as a new string, the comparisons and `<=>`, `std::hash`, `operator<<`, the conversions), and so are `wstring`, `u8string`, `u16string`, `u32string`. `sgcl::string` lives anywhere; the two kinds convert into each other and share the object ([string](string.md)). A map or a set keyed by strings is searched with a `string_view` or a literal and makes no string for the search: the hash, the equality and the order of a `string` are transparent, and the hash of a view is the hash the string keeps. A string shared between threads and replaced at run time is an `atomic<string>` (`static sgcl::atomic<sgcl::string> host;`): the atomic of the string's word, a load for the string as it was, a store for a new one, compare-exchange by identity.

```cpp
struct Element { string name; vector<tracked_ptr<Element>> children; };
string p = "p";                                // one object, made once
tracked_ptr e = make_tracked<Element>();
e->name = p;                                       // a word copied: the object shared
assert(e->name == p && e->name.object() == p.object() && e->name == "p");
map<string, int> counts;       // the hash computed once, kept in the string's object
++counts[p];
```

A piece of a string is a [slice](slice.md) that holds the string's object: the owner and a range, so `p.as_slice(1, 2)` and every piece of `p.split(',')` is a substring with no copy and no lifetime to watch, kept in a container or a managed object as it is; `std::string_view` remains the borrowed view for what takes one. A `slice<T>` is the same over any contiguous managed storage — a vector's buffer, a reader's block — and, without an owner, over unmanaged memory, where it is what `std::span` is: one type for both, the buffers of io among them.

What it costs, in nanoseconds per operation, against a `std::string` member, Go's string and Java's `String` (`benchmarks/core/string.cpp` and its Go and Java counterparts, the setup of the containers module's "Benchmarks" section ([containers](../containers/benchmarks.md)), one thread): 2 M strings of 10 and of 100 characters made from a text buffer and stored in nodes, copied from node to node, hashed once each as a map key would be, and hashed eight times in a row (a key used again and again); `sgcl::string` in the same managed nodes:

| operation, length | `sgcl::string` | `std::string` | Go string | Java `String` |
|---|---|---|---|---|
| make, 10 | 10.2 | 2.9 | 10.1 | 23.3 |
| make, 100 | 18.0 | 20.6 | 22.3 | 32.4 |
| copy, 10 | 1.9 | 1.9 | 1.3 | 1.1 |
| copy, 100 | 1.9 | 24.0 | 1.4 | 6.6 |
| hash once, 10 | 2.8 | 1.8 | 5.4 | 5.0 |
| hash once, 100 | 12.3 | 11.4 | 8.7 | 14.6 |
| hash 8 times, 10 | 0.9 | 1.8 | 5.5 | 0.9 |
| hash 8 times, 100 | 2.3 | 9.4 | 7.3 | 1.9 |

Below its small buffer (22 characters in libc++, 15 in libstdc++ and MSVC) a `std::string` costs no allocation, and no type that allocates can match that: a `string` of a few characters costs a managed allocation to make (README: "Allocation"), as a Go string does. Past the buffer, a `std::string` costs an allocation to make, an allocation and a copy per copy (24 ns for 100 characters), and a `free` in the sweep for each; a `string` costs the same allocation once, a word and the barrier per copy (1.9 ns, whatever the length, the same as a small `std::string`'s 24 bytes), and nothing in the sweep. A copy in Go is two words without a barrier while the collector is idle; in Java a reference through ZGC's store barrier. The first hash reads the characters everywhere (the `string`'s is `std::hash` of the characters, plus the store that keeps it: a nanosecond over `std::string`'s); the `string` and Java's `String` keep it in the object and pay a load from then on (the eight-times row: one computation and seven loads), where `std::string` and Go read the characters every time. What the table does not show is the sweep: two million nodes with 100-character strings freed in 31 ms with `string`s and 46 ms with `std::string`s, whose buffers `free` returns one by one, and the other way round for ten characters (24 ms against 17: two million more objects, where the `std::string`s lay inside their nodes). So `std::string` remains the member for text of a few characters made and dropped, and `string` is the member for text that is kept, shared and compared: names, keys, symbols, the leaves of a document.

## Weak pointers
`weak_ptr<T>` is a pointer the collector does not follow: the object lives as long as something else reaches it, and `lock()` says which. It is one word, a `tracked_ptr` to a small cell on the managed heap that holds the target as a word the collector clears instead of tracing; a `weak_ptr` made from a `tracked_ptr` gets a cell of its own, copies share it, and the cell is collected with the last copy. It lives wherever a `tracked_ptr` may, and threads share it the way they share a `tracked_ptr` (rule 6).

```cpp
struct Item { string name; };
tracked_ptr item = make_tracked<Item>("x");
weak_ptr cached = item;                  // a cell, allocated once
if (auto p = cached.lock()) {                  // the Item, held by p
    p->name = "y";
}
item = nullptr;                                // unreachable now
collector::force_collect(true);          // optional, for the demonstration only: the next cycle clears it anyway
assert(cached.expired() && !cached.lock());    // cleared, never dangling
```

The clearing is a phase of the cycle: once the marking has converged, every cell whose target the cycle found unreachable has its word cleared, before the sweep. So `lock()` never hands out an object the sweep will destroy or the slot it will be reused for; and when `lock()` races with the clearing it either sees the null or wins, holding the object for at least one more cycle, through the same hazard pointer as `atomic<tracked_ptr>::load` (a cell is cleared before the collector reads the hazards, a lock publishes its hazard before it reads the cell again). `expired()` is true once the cell is cleared; between the object becoming unreachable and the cycle that notices, `lock()` still returns it, which is the same lag as any garbage collector's. A program without weak pointers pays nothing: the phase is a test of an empty list.

Nanoseconds per operation against `std::weak_ptr`, Go's `weak.Pointer` and Java's `WeakReference`, one thread and four threads on objects of their own (`benchmarks/core/weak_ptr.cpp` and its Go and Java counterparts, the setup of the "Benchmarks" section):

| operation, threads | `sgcl::weak_ptr` | `std::weak_ptr` | Go `weak.Pointer` | Java `WeakReference` |
|---|---|---|---|---|
| lock, 1 | 1.8 | 12.1 | 6.1 | 1.0 |
| lock, 4 | 1.9 | 12.7 | 6.2 | 1.1 |
| lock of an expired pointer, 1 | 0.9 | 1.4 | 5.2 | 1.0 |
| lock of an expired pointer, 4 | 1.0 | 1.5 | 4.0 | 1.1 |
| copy, 1 | 1.4 | 8.6 | 6.3 | 0.8 |
| copy, 4 | 1.5 | 8.8 | 6.4 | 0.9 |
| make from a strong pointer, 1 | 7.4 | 8.5 | 18.4 | 3.6 |
| make from a strong pointer, 4 | 7.8 | 8.8 | 18.7 | 7.2 |

A lock is the cell read twice around a hazard store and a `tracked_ptr` built; on an expired pointer the same reads find the null and build a null pointer; a copy is a `tracked_ptr` copy; making one is a 16-byte allocation. `std::weak_ptr` pays two atomic count updates per lock and per copy, and contended ones when threads share an object. Go's `weak.Pointer` is a handle the runtime hands out and resolves in a call (`Value`), and making one allocates the handle; Java's `WeakReference` is an object whose referent is read through ZGC's load barrier, the cheapest lock of the four, and making one is an allocation the collector has to discover.

### expiry_queue
A destructor is the object's own business and runs on the collector's threads under the rules of destructors; what an observer wants done with an object once nothing else reaches it (a handle released, a cache entry dropped, a registry told, or the object kept after all) goes into an `expiry_queue<T>`. `watch(object, f)` makes a weak cell for the object and keeps `f` next to it, and returns the entry's handle (`cancel()`, or a `weak_ptr` to the object). When a cycle finds the object unreachable it does not destroy it: it keeps it alive for the queue, and `drain()` calls `f` with the object as a `tracked_ptr`, alive one last time, on the thread that calls `drain()`, at that moment, with the heap in a consistent state. `f` may read the object, release what it owns, or keep the pointer, which is the object's return to life (it can be watched again). Then the entry is dropped and the object dies with the next cycle that finds it unreachable, its destructor as ever. Until `drain()` the object stays alive, and its `weak_ptr`s lock it; the queue drains by itself every so many `watch` calls, as many as it has entries, and a thread that watches little and wants its cleanups on time calls `drain()` in its loop. `f` is an `sgcl::function` ([variant, any, function and expected](README.md#variant-any-function-and-expected)): its closure may capture tracked pointers, which the collector follows; a closure holding a strong pointer to the watched object itself keeps it alive, and the entry never expires, which is why the object comes as the argument. Java's `Cleaner` and Go's `AddCleanup` do the cleanup on a thread of the runtime's and never show the object; here the program says where and when, and gets the object.

```cpp
struct Texture { GLuint id; };
expiry_queue<Texture> gone;                        // on the stack, or inside a managed object

tracked_ptr texture = make_tracked<Texture>(upload(pixels));
auto entry = gone.watch(texture, [](tracked_ptr<Texture> t) { glDeleteTextures(1, &t->id); });
// ... the texture is used, shared, dropped by everyone; or freed by hand: entry.cancel(), and the function is never called
gone.drain();                                            // in the render loop: the GL name freed on this thread, the object destroyed by a later cycle
```

The cost, for a program without such queues, is a test of an empty list per cycle; with them, a pass over the cells per convergence of the marking and one more round of marking for the objects kept, plus their memory until the drain.

## The rules
Everything the collector relies on, in one place; the sections below say why.

1. An `sgcl::tracked_ptr`, and every `sgcl` type that holds one (a container, an atomic, a weak pointer, a coroutine handle), lives inside a managed object or on a stack: never in `new`/`malloc` memory, a standard container, a global, a `thread_local`, a lambda copied to the heap, or the frame of a coroutine, unless the coroutine's promise derives from `managed_frame` ([Coroutines](../async/README.md#coroutines)). Debug builds assert it. A `root_ptr` lives anywhere: it holds its object through a managed cell ([The pointer and its places](README.md#the-pointer-and-its-places)).
2. A `tracked_ptr` does not share storage with data: no `union` with a value, no `std::variant`, no small-buffer `std::function` or `std::any` holding one; `sgcl::variant`, `any`, `function` and `expected` are the ones that keep it apart. A union of two `tracked_ptr`s and `std::optional<tracked_ptr<T>>` are fine.
3. A raw pointer, a reference or an iterator keeps nothing alive by itself; it is valid while a `tracked_ptr` or a container keeps its target, as with `std`.
4. A `tracked_ptr` addresses a managed object or a part of it (a member, a base), never an element of a container's buffer. A `weak_ptr` follows the rules of a `tracked_ptr` (it is one, to a cell) and addresses an object no `unique_ptr` owns.
5. A destructor reads its object's `tracked_ptr` members only through `if_alive()`; its `unique_ptr` members it may use freely.
6. Objects shared between threads are shared the way any C++ objects are: a `tracked_ptr` written by one thread and read by another needs `atomic` or `atomic_ref`, or the program's own synchronization. The word itself is atomic, so a race on it is never a torn pointer, and the collector is correct under any interleaving.

A global root is a `unique_ptr`: the object it owns is reachable, and so is everything reachable from it, for as long as the global lives. When the global has to point at an object that other threads share and that is replaced at run time (a current configuration, a snapshot), a `unique_ptr` is the wrong shape, since assigning it destroys the old object at once, under the threads still using it; the root is then a `root_ptr`, under an `atomic_ref` when the replacement races with the readers. Readers `load()`, a writer `store()`s, and the old configuration is collected when the last reader drops it:

```cpp
static root_ptr<Config> current;                        // the root, for the life of the program: a global

atomic_ref a(current);                                  // the atomic of the root: the word of its cell
tracked_ptr<Config> config = a.load();                  // a reader: held until dropped
a.store(make_tracked<Config>(...));                     // a writer: the old one lives on for its readers
```

The same with the atomic inside is a `unique_ptr` to a managed object holding it: `static sgcl::unique_ptr current = sgcl::make_tracked<sgcl::atomic<sgcl::tracked_ptr<Config>>>();`, read and written through `current->`.

A managed object held from anywhere else in unmanaged memory (a `std::vector`, a `new`ed object, a lambda run on another thread) is a `root_ptr`, or a `std::shared_ptr` from `to_shared()` where the holder has to be a `shared_ptr`: its control block owns a managed holder of the pointer, a root that lives exactly as long as the last copy of the `shared_ptr`, and the object stays managed, destroyed on the collector's threads once nothing reaches it. Two allocations per call, so a named function, not a conversion:

```cpp
std::vector<root_ptr<Node>> kept;               // a std container: no tracked_ptr may live in it
tracked_ptr node = make_tracked<Node>();
kept.emplace_back(node);                              // a root_ptr: a cell on the managed heap, the Node's root
std::vector<std::shared_ptr<Node>> shared;
shared.push_back(node.to_shared());                   // or a shared_ptr: the Node lives while it does
```

## Pages

| page | header | what it is |
|---|---|---|
| [tracked_ptr](tracked_ptr.md) | `tracked_ptr.h` | the pointer the collector follows: one word, a write barrier, no count; aliases, `type()`, `is<U>()`, `as<U>()`, `if_alive()` |
| [unique_ptr](unique_ptr.md) | `unique_ptr.h` | what `make_tracked` returns: a `std::unique_ptr` to a managed object, deterministic until moved into a `tracked_ptr` |
| [make_tracked](make_tracked.md) | `make_tracked.h` | creates an object on the managed heap |
| [root_ptr](root_ptr.md) | `root_ptr.h` | a root that lives anywhere (a global, a `std` container, a lambda on the heap): a cell of a managed block under it, the `tracked_ptr` it holds its object by one step away; the pointer of an interpreter's handle table or a program's globals |
| [weak_ptr](weak_ptr.md) | `weak_ptr.h` | a pointer that does not keep its object alive, cleared by the cycle that finds the object unreachable |
| [collector](collector.md) | `collector.h` | `force_collect`, `terminate`, statistics and phase times, live objects and bytes by type, the memory limit |
| [variant](variant.md) | `variant.h` | `std::variant`'s interface with the tracked pointers in a word of their own, apart from the data of the other alternatives |
| [any](any.md) | `any.h` | `std::any`'s interface with a tracked pointer in a word of its own and an object with pointers in a managed node of its own |
| [function](function.md) | `function.h` | `std::function` and `std::move_only_function` whose closure may capture tracked pointers: the closure in a managed node of its own |
| [expected](expected.md) | `expected.h` | `std::expected`'s interface (C++23) over a variant: the value and the error laid out apart |
| [string](string.md) | `string.h` | an immutable string on the managed heap: one word, shared by copying, compared and hashed by its contents, no destructor; `split`, `join`, `trim`, `replace` as Go's `strings` has them |
| [slice](slice.md) | `slice.h` | the elements of a contiguous range and the managed object they lie in, held: `s.as_slice(pos, n)`, the pieces of `split`, `v.as_slice()`, a reader's lines; without an owner a `std::span`; `slice<const char>` with the text interface |
| [range](range.md) | `range.h` | a pair of iterators as a range (what `equal_range` hands back, made iterable) and the integers of `range(n)`, `range(first, last)`; for a range-for and `std::ranges` |
| [optional, pair, tuple](aliases.md) | `aliases.h` | the `std` types under the library's names: they hold a tracked pointer correctly as they are, one value per place |
| [config](config.md) | `config.h` | the compile-time constants and the `-D` macros that set them |
| [mixin/](mixin/README.md) | `mixin/mixin.h` | the mixins, `namespace mixin`: what a container declares by its bases, and the methods it gets for it; a README of their own |
| [mixin::enumerable](mixin/enumerable.md) | `mixin/mixin::enumerable.h` | the questions asked of the elements of any range of the library: `find_if`, `exists`, `count_of`, `contains`, `index_of`, `min`, `max` |
| [mixin::equatable, mixin::comparable](mixin/equatable.md) | `mixin/mixin::equatable.h`, `mixin/mixin::comparable.h` | `==` and `<=>` between two containers, by the elements; [mixin::comparable](mixin/comparable.md) |
| [mixin::ordered](mixin/ordered.md) | `mixin/mixin::ordered.h` | the order of a range: `is_sorted`, `binary_search`, `lower_bound`, `sort`, `sort_by`, `stable_sort` |
| [mixin::sequence](mixin/sequence.md) | `mixin/mixin::sequence.h` | the writes over a range: `fill`, `reverse` |
| [mixin::lookup](mixin/lookup.md) | `mixin/mixin::lookup.h` | a map by its key: `get`, `try_get`, `value_or`, `contains_key`, `keys`, `values` |
| [mixin::text](mixin/text.md) | `mixin/mixin::text.h` | the read side of `std::string_view` for `string` and `slice<const CharT>` |
| [req](req.md) | `req.h` | the requirements, `namespace req`: `enumerable`, `ordered`, `sequence`, `lookup`, `comparable`, ... — what a parameter asks for; nominal for a container, structural for a value |
