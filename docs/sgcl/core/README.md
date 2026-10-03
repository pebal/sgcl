[sgcl](../README.md) › core

# sgcl::core

```cpp
#include "sgcl/core.h"   // namespace sgcl
```

The collector and the pointers, the value types and the containers: what every other module is built on, and
everything that stands in `sgcl::` itself. The module depends on nothing but the standard library. Every other
module is a directory and a namespace of its own ([the modules](../README.md)).

The word by which every type of the library holds its memory is `tracked_ptr<T>`: the pointer the collector
follows, one word. The collector finds such words in two places only, inside managed objects and on the stacks of
threads, so a `tracked_ptr`, and every type that holds one (a container, a `string`, an atomic, a weak pointer),
lives there and nowhere else. An object is made on the managed heap by `make_tracked`, which returns a
`unique_ptr`; the collector takes the object over once it is moved into a `tracked_ptr`. What has to live
elsewhere (a global, a `std` container, a lambda on the heap) holds its object by a `root_ptr`, and a handle of the
library, a value of one tracked word such as a `string` or an `io::file`, by a `rooted`.

Around the pointer the module gives the value types that keep a tracked pointer apart from data (`variant`, `any`,
`function`, `expected`), the immutable `string` and the `slice` that holds what it views, the containers of the
standard library with their memory on the managed heap, the weak pointer and the containers keyed by objects they
do not keep alive, `atomic` over a pointer or a handle, `duration` and `clock`, the coroutine frames on the
managed heap, and the mixins and requirements every container of the library declares itself by. The engine under
it (the heap, the barrier, the roots, a cycle phase by phase) has a chapter of its own,
[the garbage collector](../../garbage_collector/README.md); the code is in `sgcl/core/detail/`.

## The rules

Everything the collector relies on, in one place.

1. An `sgcl::tracked_ptr`, and every `sgcl` type that holds one (a container, an atomic, a weak pointer, a coroutine handle, a `string`), lives inside a managed object or on a stack: never in `new`/`malloc` memory, a standard container, a global, a `thread_local`, a lambda copied to the heap (`std::function`, `std::thread`, `std::async`, `std::packaged_task`; `sgcl::function` and `sgcl::thread` keep the closure in a managed node), a `std::shared_ptr`'s object, an exception object (the runtime allocates it: an exception carries its message as a `std::string`, `runtime_error(msg.c_str())`, and a value with a tracked pointer in a [rooted](rooted/README.md) member, as `bad_expected_access` does), or the frame of a coroutine, unless the coroutine's promise derives from `managed_frame` ([Coroutines](../async/README.md#coroutines)). A stack is the stack of a thread, which the collector registers the first time it copies a pointer; an alternate signal stack, a fiber or a `ucontext` is memory the scan never sees, and the library's coroutines are the one form of many stacks it has. Debug builds assert all of it. A `root_ptr` lives anywhere: it holds its object through a managed cell ([root_ptr](root_ptr/README.md)); a `rooted<T>` is a value kept that way.
2. A `tracked_ptr` does not share storage with data: no `union` with a value, no `std::variant`, no small-buffer `std::function` or `std::any` holding one; `sgcl::variant`, `any`, `function` and `expected` are the ones that keep it apart. A union of two `tracked_ptr`s and `std::optional<tracked_ptr<T>>` are fine.
3. A raw pointer, a reference or an iterator keeps nothing alive by itself; it is valid while a `tracked_ptr` or a container keeps its target, as with `std`.
4. A `tracked_ptr` addresses a managed object or a part of it (a member, a base), never an element of a container's buffer. A `weak_ptr` follows the rules of a `tracked_ptr` (it is one, to a cell) and addresses an object no `unique_ptr` owns.
5. A destructor reads its object's `tracked_ptr` members only through `if_alive()`; its `unique_ptr` members it may use freely.
6. Objects shared between threads are shared the way any C++ objects are: a `tracked_ptr` written by one thread and read by another needs `atomic` or `atomic_ref`, or the program's own synchronization. The word itself is atomic, so a race on it is never a torn pointer, and the collector is correct under any interleaving.

A global, a `std` container or any other unmanaged memory holds a managed object by a [root_ptr](root_ptr/README.md), a
handle by a [rooted](rooted/README.md), and a holder that has to be a `std::shared_ptr` by
[to_shared()](tracked_ptr/README.md).

### Containers

The containers of the standard library with their nodes and buffers on the managed heap and the observers built on them (`weak_map`, `weak_set`, `expiry_queue`): a container lives where a `tracked_ptr` may live, its elements are destroyed exactly when `std` destroys them, and the collector reclaims the memory. Each has a header of its own in `sgcl/core/` (`sgcl/core/vector.h`, `sgcl/core/sorted_map.h`...), and `sgcl/core.h` brings them all in; the containers against `std`, in numbers, are on [benchmarks](benchmarks.md). The immutable containers are a module of their own, [immutable](../immutable/README.md).

`vector`, `array`, `dynamic_array`, `deque`, `list`, `forward_list`, `map`, `set`, `multimap`, `multiset`, `sorted_map`, `sorted_set`, `sorted_multimap`, `sorted_multiset`, `ordered_map`, `ordered_set` and the adapters `stack`, `queue`, `priority_queue` follow the interfaces of their `std` counterparts, including iterator categories (`std::ranges` algorithms work on them), transparent lookup, node handles, `std::erase`/`std::erase_if`, and three-way comparison. The names go by the order of iteration, not by the history of the standard: `map` and `set` are the hash containers (`std::unordered_map`, `std::unordered_set`), the default in every language of the last twenty years; `sorted_map` and `sorted_set` are the trees (`std::map`, `std::set`), iterated in the order of the keys, with `lower_bound` and `upper_bound`; `ordered_map` and `ordered_set` are hash containers iterated in the order of insertion (Java's `LinkedHashMap`). Code that comes from `std::map` and needs the keys in order goes to `sorted_map`. The containers differ from the standard ones in where their memory lives and when elements die:

- A container holds its buffer or its root node by a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, never in `new`/`malloc` memory or in a standard container ([The rules](#the-rules), 1). Iterators are plain pointers, valid exactly when their `std` counterparts are, and may live anywhere: the container roots every element it holds, and a raw pointer in a stack frame is a root of its own under the conservative scan.
- Nodes and buffers are managed objects: an `erase` unlinks a node and the collector reclaims it later; nothing is ever freed by hand, so a cycle through a container is collected like any other cycle.
- The collector reads the elements only where they may hold pointers. A buffer whose element type cannot hold a `tracked_ptr` (trivially default constructible, or smaller than a pointer: `int`, `double`, a plain struct) gets an empty pointer map when it is created, so the marking never reads its contents: a `vector<int>` of a million elements costs a cycle what one object does, and its buffer is not even zeroed on allocation. A node holds links and an element; the words of an element that turn out to be data leave the node type's map at the first node found holding some ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)), and from then on the marking reads the links alone.
- The node containers (`list`, `forward_list`, the maps and sets) destroy an element the moment it is erased, cleared, assigned over or the container is destroyed, exactly like `std`. An iterator to an erased element is invalid as in `std`; it keeps the node's memory mapped but not the element. The one exception is a container dying in a sweep, inside a managed object nobody refers to any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it.
- `vector` and `array` destroy their elements themselves, exactly when `std` does: on removal (`erase`, `pop_back`, `clear`, `resize`, `assign`), on a reallocation (the moved-from elements), in the destructor, wherever that runs, on a stack or in a sweep inside a dying managed object. The collector never destroys a buffer: it only frees one nothing refers to, and it never needs to know how many elements a buffer holds. A buffer is referred to only through a pointer to its first element: a `tracked_ptr` or reference to an element does not keep it, so once the container is gone such a pointer dangles, as in `std`. `clear()` keeps the capacity, like `std::vector`; `shrink_to_fit()` on an empty vector drops the buffer. A `vector` is three words (the buffer, the count, the capacity), a `dynamic_array<T>` two; the buffer's own header holds only its metadata and the capacity the size class granted.
- `array<T, N>` keeps its elements inline, like `std::array`: an aggregate (`sgcl::array<sgcl::tracked_ptr<T>, 4> roots = {}`) with the tuple interface and costs nothing beyond the elements. `array<T>` (no `N`) is a buffer whose size is fixed when it is created (`array<T>(n)`, `array<T>(n, value)`, from a range or an initializer list): the cheapest managed sequence, a single word to hold, copied deeply and moved by handing the buffer over.
- Elements aligned beyond 16 bytes are not supported in buffers; `vector<bool>` is a plain vector of `bool`.
- Past `std`: `ordered_map` and `ordered_set` are the hash containers iterated in insertion order (Java's `LinkedHashMap` and `LinkedHashSet`): the same table with every node on one more list, `front` the oldest element and `back` the newest, `to_back` and `to_front` to move one, which makes a cache with an eviction order (LRU) two lines over the map ([ordered_map](ordered_map/README.md)).

## Pointers

| Pointer | Header | Description |
|---|---|---|
| [atomic\<H\>](atomic-handle/README.md) | `atomic.h` | the atomic of a handle's word (`string`, `io::file`, a channel): one word, compare-exchange by identity |
| [atomic\<T\>](atomic.md) | `atomic.h` | the atomic of the library: `std::atomic<T>` for every `T` but a `tracked_ptr` and a handle, which have specializations of their own |
| [atomic\<tracked_ptr\<T\>\>](atomic-tracked_ptr/README.md) | `atomic.h` | the lock-free atomic `tracked_ptr`: `load`, `store`, compare-exchange, `wait`/`notify`, no ABA |
| [atomic_ref\<H\>](atomic_ref-handle/README.md) | `atomic_ref.h` | the atomic view of a handle's word that is not declared atomic |
| [atomic_ref\<T\>](atomic_ref.md) | `atomic_ref.h` | the atomic view of a variable not declared atomic: `std::atomic_ref<T>` for every `T` but a `tracked_ptr` and a handle |
| [atomic_ref\<tracked_ptr\<T\>\>](atomic_ref-tracked_ptr/README.md) | `atomic_ref.h` | the atomic view of a `tracked_ptr` that is not declared atomic |
| [root_ptr\<T\>](root_ptr/README.md) | `root_ptr.h` | a root that lives anywhere (a global, a `std` container, a lambda on the heap): a cell of a managed block under it, the `tracked_ptr` it holds its object by one step away; the pointer of an interpreter's handle table or a program's globals |
| [rooted\<T\>](rooted/README.md) | `rooted.h` | a value with tracked pointers inside kept in a managed object of its own under a root: what an exception object, a `std` container, a global or a platform's closure holds instead of the value; `bad_expected_access` carries its error so |
| [tracked_ptr\<T\>](tracked_ptr/README.md) | `tracked_ptr.h` | the pointer the collector follows: one word, a write barrier, no count; aliases, `type()`, `is<U>()`, `as<U>()`, `if_alive()` |
| [unique_ptr\<T\>](unique_ptr/README.md) | `unique_ptr.h` | what `make_tracked` returns: a `std::unique_ptr` to a managed object, deterministic until moved into a `tracked_ptr` |
| [weak_ptr\<T\>](weak_ptr/README.md) | `weak_ptr.h` | a pointer that does not keep its object alive, cleared by the cycle that finds the object unreachable |

## Functions

| Function | Header | Description |
|---|---|---|
| [make_any](make_any.md) | `any.h` | an `any` holding a `T` made from the arguments in place |
| [make_tracked\<T\>](make_tracked.md) | `make_tracked.h` | creates an object on the managed heap |
| [to_array](to_array.md) | `array.h` | an `array<T, N>` from a built-in array, its elements copied or moved |

## Classes

| Class | Header | Description |
|---|---|---|
| [any](any/README.md) | `any.h` | `std::any`'s interface with a tracked pointer in a word of its own and an object with pointers in a managed node of its own |
| [bad_expected_access\<E\>](bad_expected_access/README.md) | `expected.h` | what `expected::value()` throws when it holds an error, the error inside |
| [clock](clock/README.md) | `clock.h` | `clock::now()`, the library's time in one place: the steady clock's, or a test's `manual_clock`'s while one is installed; `time_point` |
| [collector](collector/README.md) | `collector.h` | `force_collect`, `terminate`, statistics and phase times, live objects and bytes by type, the memory limit |
| [config](config.md) | `config.h` | the compile-time constants and the `-D` macros that set them |
| [counting_iterator\<T\>](counting_iterator.md) | `range.h` | the iterator of `range(n)`: an integer whose `*` is the number, random access to `std::ranges` |
| [duration](duration/README.md) | `duration.h` | a span of time: nanoseconds in 64 bits, Go's text both ways (`"1h30m"`, `parse`), `seconds()` and the other units, saturated arithmetic; converts from every integral `std::chrono` duration and into `std::chrono::nanoseconds` |
| [duration_error](duration_error/README.md) | `duration.h` | what `duration::parse` reports about text that is not a duration |
| [expected\<T, E\>](expected/README.md) | `expected.h` | `std::expected`'s interface (C++23) over a variant: the value and the error laid out apart |
| [frame_ptr\<Promise\>](frame_ptr/README.md) | `coroutine.h` | the owner of a coroutine with a managed frame, which lives anywhere ([coroutines](coroutine.md)) |
| [function\<R(Args...)\>](function/README.md) | `function.h` | `std::function` and `std::move_only_function` whose closure may capture tracked pointers: the closure in a managed node of its own |
| [generator\<T\>](generator/README.md) | `generator.h` | a coroutine that `co_yield`s values, consumed with a range-for or `next()`, its frame managed; runs where it is called, no scheduler |
| [managed_frame](managed_frame.md) | `coroutine.h` | the base of a promise whose coroutine frame is on the managed heap, its parameters and locals roots while the frame is held ([coroutines](coroutine.md)) |
| [move_only_function\<R(Args...)\>](move_only_function/README.md) | `function.h` | `std::move_only_function` whose closure may capture tracked pointers |
| [optional\<T\>, pair\<T1, T2\>, tuple\<Ts...\>](aliases.md) | `aliases.h` | the `std` types under the library's names: they hold a tracked pointer correctly as they are, one value per place |
| [range\<It\>](range/README.md) | `range.h` | a pair of iterators as a range (what `equal_range` hands back, made iterable) and the integers of `range(n)`, `range(first, last)`; for a range-for and `std::ranges` |
| [slice\<T\>](slice/README.md) | `slice.h` | the elements of a contiguous range and the managed object they lie in, held: `s.as_slice(pos, n)`, the pieces of `split`, `v.as_slice()`, a reader's lines; without an owner a `std::span`; `slice<const char>` with the text interface |
| [string](string/README.md) | `string.h` | an immutable string on the managed heap: one word, shared by copying, compared and hashed by its contents, no destructor; `split`, `join`, `trim`, `replace` as Go's `strings` has them; UTF-8, with `runes()`, a `char32_t` as a character and Unicode's case and white space |
| [thread](thread/README.md) | `thread.h` | `std::thread` with the callable and the arguments in a managed node of their own: a `tracked_ptr` captured by value as in a `function`; `sgcl::this_thread` is `std::this_thread` |
| [unexpected\<E\>](unexpected/README.md) | `expected.h` | the error an `expected` is made from: `return unexpected(e);` |
| [utf8, unicode, runes](utf8/README.md) | `utf8.h`, `unicode.h` | the encoding (`decode`, `encode`, `count`, `valid`), the code point's case and white space (`to_lower`, `is_space`, `equal_fold`, from generated tables), the code points of a text as a range of the library |
| [variant\<Ts...\>](variant/README.md) | `variant.h` | `std::variant`'s interface with the tracked pointers in a word of their own, apart from the data of the other alternatives |

## Sequences and associative containers

| Container | Header | `std` counterpart |
|---|---|---|
| [array\<T, N\>](array/README.md) | `array.h` | `std::array`, with the braces of an aggregate and the mixins of a range |
| [deque\<T\>](deque/README.md) | `deque.h` | `std::deque` |
| [dynamic_array\<T\>](dynamic_array/README.md) | `dynamic_array.h` | a count fixed at creation in a managed buffer that never moves: Java's `new T[n]`; the rings of the channels |
| [forward_list\<T\>](forward_list/README.md) | `forward_list.h` | `std::forward_list` |
| [list\<T\>](list/README.md) | `list.h` | `std::list` |
| [map\<Key, T, Hash, KeyEqual\>](map/README.md) | `map.h` | `std::unordered_map` |
| [multimap\<Key, T, Hash, KeyEqual\>](multimap/README.md) | `multimap.h` | `std::unordered_multimap` |
| [multiset\<Key, Hash, KeyEqual\>](multiset/README.md) | `multiset.h` | `std::unordered_multiset` |
| [ordered_map\<Key, T, Hash, KeyEqual\>](ordered_map/README.md) | `ordered_map.h` | `map` iterated in insertion order: Java's `LinkedHashMap`; `front`, `back`, `to_back`, `to_front` |
| [ordered_set\<Key, Hash, KeyEqual\>](ordered_set/README.md) | `ordered_set.h` | `set` iterated in insertion order: Java's `LinkedHashSet` |
| [priority_queue\<T, Container, Compare\>](priority_queue/README.md) | `queue.h` | `std::priority_queue` |
| [queue\<T, Container\>](queue/README.md) | `queue.h` | `std::queue` |
| [set\<Key, Hash, KeyEqual\>](set/README.md) | `set.h` | `std::unordered_set` |
| [sorted_map\<Key, T, Compare\>](sorted_map/README.md) | `sorted_map.h` | `std::map` |
| [sorted_multimap\<Key, T, Compare\>](sorted_multimap/README.md) | `sorted_multimap.h` | `std::multimap` |
| [sorted_multiset\<Key, Compare\>](sorted_multiset/README.md) | `sorted_multiset.h` | `std::multiset` |
| [sorted_set\<Key, Compare\>](sorted_set/README.md) | `sorted_set.h` | `std::set` |
| [stack\<T, Container\>](stack/README.md) | `stack.h` | `std::stack` |
| [vector\<T\>](vector/README.md) | `vector.h` | `std::vector` |

The questions, the order and the writes of a range (`contains`, `index_of`, `find_if`, `sort`, `reverse`, `min`, `for_each`...) are members of every container that iterates, from the mixins of `core` ([the mixins and the requirements](mixin/README.md)); the maps read by their key through [mixin::lookup](mixin/lookup/README.md).

## Weak containers

| Container | Header | Description |
|---|---|---|
| [expiry_queue\<T\>](expiry_queue/README.md) | `expiry_queue.h` | a callback for an object the collector found unreachable, with the object alive again for the call |
| [weak_map\<Key, T\>](weak_map/README.md) | `weak_map.h` | values attached to objects the map does not keep alive: keyed by the object, an entry dies with it |
| [weak_multimap\<Key, T\>](weak_multimap/README.md) | `weak_map.h` | the same with several values per object |
| [weak_set\<Key\>](weak_set/README.md) | `weak_set.h` | a set of objects it does not keep alive |

## Immutable containers

A module of their own, in the namespace `sgcl::immutable` and the directory `sgcl/immutable/`, with a [README](../immutable/README.md) that is their guide (the model: every operation a new version sharing all but the path it changed; the state of a program as a value): [immutable::vector](../immutable/vector/README.md), [immutable::list](../immutable/list/README.md), [immutable::map](../immutable/map/README.md), [immutable::set](../immutable/set/README.md), and their [benchmarks](../immutable/benchmarks.md) against immer and `std`.

## Mixins

The bases a container of the library declares itself by, and the members each gives it
([the mixins](mixin/README.md), `namespace sgcl::mixin`).

| Mixin | Description |
|---|---|
| [bidirectional\<Derived\>](req/bidirectional.md) | a declaration without methods: the range is walked backwards as well |
| [comparable\<Derived\>](mixin/comparable/README.md) | `<=>` between two containers, by the elements |
| [contiguous\<Derived\>](req/contiguous.md) | a declaration without methods: the elements lie next to each other in memory |
| [enumerable\<Derived\>](mixin/enumerable/README.md) | the questions asked of the elements: `contains`, `index_of`, `find_if`, `count_of`, `min`, `max`, `for_each` |
| [equatable\<Derived\>](mixin/equatable/README.md) | `==` between two containers, by the elements |
| [immutable\<Derived\>](mixin/immutable.md) | a declaration without methods: a value that never changes, every change a new container |
| [lookup\<Derived\>](mixin/lookup/README.md) | a map read by its key: `get`, `try_get`, `value_or`, `contains_key`, `keys`, `values` |
| [ordered\<Derived\>](mixin/ordered/README.md) | the order of the elements: `is_sorted`, `binary_search`, `lower_bound`, `sort`, `sort_by`, `stable_sort` |
| [random_access\<Derived\>](req/random_access.md) | a declaration without methods: the elements are reached by position |
| [sequence\<Derived\>](mixin/sequence/README.md) | the writes over the elements: `fill`, `reverse` |
| [text\<Derived, CharT, Traits\>](mixin/text/README.md) | the read side of `std::string_view` for `string` and `slice<const CharT>` |

## Requirements

The concepts a parameter of a function asks for ([req](req/README.md), `namespace sgcl::req`).

| Requirement | Description |
|---|---|
| [bidirectional](req/bidirectional.md) | a range walked backwards as well |
| [comparable](req/comparable.md) | a value with an order: `<=>`, or `<` alone |
| [contiguous](req/contiguous.md) | a range whose elements lie next to each other in memory |
| [enumerable](req/enumerable.md) | a range of the library: carries the mixin `enumerable` |
| [equatable](req/equatable.md) | a value with `==` |
| [handle](req/handle.md) | a public type of one tracked word to the object inside it |
| [immutable](req/immutable.md) | a range that never changes: every change a new one |
| [lookup](req/lookup.md) | a map read by its key |
| [ordered](req/ordered.md) | a range of comparable elements, with the questions of their order |
| [random_access](req/random_access.md) | a range whose elements are reached by position |
| [sequence](req/sequence.md) | a range whose elements may be written in place |

## See also

- [Benchmarks](benchmarks.md): the containers, `string` and `weak_ptr` against `std`, Go and Java
- [The garbage collector](../../garbage_collector/README.md): how the engine works, its diagnostics and
  benchmarks
- [The modules](../README.md)
