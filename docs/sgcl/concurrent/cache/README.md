[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>

```cpp
#include "sgcl/concurrent/cache.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class cache;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::cache<Key, T, Hash, KeyEqual>` is a key-value cache shared by any number of threads, bounded by a
capacity (a number of entries) and, if asked, by a time to live, evicting the entries least recently used: what
Guava's `Cache` and Caffeine are in Java (the standard libraries of Go and Java have none, and everybody writes
one), and the concurrent counterpart of the LRU cache that [ordered_map](../../core/ordered_map/README.md) gives in two lines
to one thread. It is built over [concurrent::map](../map/README.md), the split-ordered hash map, so that `get` is the map's
wait-free search and nothing else that is shared: no lock, no list to relink, no counter every thread writes.

An exact LRU keeps its entries on a list and moves one to the front on every access, which is a write to a shared
structure on every hit, the one thing a cache read by many threads cannot afford (Caffeine's lesson, and the
reason it buffers its reads); so the order here is approximated, the way Redis approximates it. Every entry keeps
a stamp of its last access: the value of a clock that the cache's insertions tick, a counter `put` advances and
`get` reads. A hit stores the current tick into its entry, one relaxed store, and only when the tick differs from
the one already there, so the line of an entry read over and over stays shared between the cores. When an
insertion takes the size past the capacity, the thread that inserted evicts by sampling: it walks `sample` entries
on from where its last walk ended (a cursor per stripe of threads, so that threads evicting at once walk different
stretches of the map's list), erases the stale ones on the way, and erases the oldest stamp among the rest; and
again, until the size is at the capacity.

The ticks are exactly as fine as the evictions need. Two entries used between the same two insertions are equally
recent; an entry put is as recent as the uses just before it and older than any use after; an entry untouched over
the last *n* insertions is *n* ticks old. With the default sample of 8 the entry evicted is older than seven others
at least, so what survives is what was used since most of the cache was inserted, which is what LRU is for: a
working set read over and over survives a stream of entries read once, and a cache of two behaves exactly as the
list would ([put](put.md)). Sampling more approximates the list closer at a longer eviction;
`ordered_map` under a mutex is the one to reach for when the order has to be exact.

The value lives in the map's node, copied in by `put` and never modified there; a `put` on a key already present
puts the new value in a box of its own that the entry points to, so there is no moment of absence and a `get` in
flight reads the old value whole, at one load more on the gets of that entry. A value is copied out, because the
entry may be evicted by another thread the moment after, so the natural payload is a `tracked_ptr` or a
[string](../../core/string/README.md), one word each. What differs from Guava's `get(key, loader)`: two threads that miss
the same key both compute it, and neither waits for the other ([get_or_compute](get_or_compute.md)).

## Rules

- Every member function may be called from any thread at any time. `get` writes nothing shared but the stamp of
  the entry it hit, when it changed, and is wait-free when it finds the entry fresh or no entry; `put`,
  `get_or_compute` and `erase` are lock-free: the map's `try_emplace` or `erase`, and the eviction the insertion
  owes, a walk of `sample` entries and an erasure per entry over the capacity. Nothing waits.
- `size()` is exact (the cache's own count, one word) and at most the capacity once the `put`s have returned; while
  several threads insert at once it may pass the capacity by the number of them, each on its way to evict. The
  count is one word every insertion and erasure writes, the tick another, each on a cache line of its own: the two
  shared writes of a `put`; a `get` reads the line of the tick.
- With a time to live an entry put more than `ttl` ago is absent: a `get` that finds it stale erases it and misses,
  `get_or_compute` computes it again, and an eviction pass erases every stale entry it walks past before it looks
  for the oldest. A `put` renews the entry: its time runs from the last `put`, not from the last `get`. The clock
  is [sgcl::clock](../../core/clock/README.md), the steady clock or the [manual_clock](../../async/manual_clock/README.md) while one is
  installed, so that a test moves a time to live on without sleeping; it is read only when there is a time to live.
- The value is copied in on `put` and out on `get`, so `T` is copy-constructible (a `static_assert` says so); a
  `put` of an rvalue moves it only into the box of a replacement.
- An entry evicted or erased is destroyed by the collector with its node, once nothing holds it: not at the
  erasure, which other threads may be reading it across ([README: Lock-free containers](../README.md#lock-free-containers)).
  The cursors hold the node each stripe's last walk ended at, so an entry erased under a cursor lives on until that
  stripe's next eviction; `clear()` lets go of them.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys: hashed by `Hash`, compared by `KeyEqual`, copied into the node of its entry. |
| `T` | The type of the values: copy-constructible. A `tracked_ptr` or a `string` is copied as one word. |
| `Hash` | The hash of the keys, `std::hash<Key>` by default. The bucket is the low bits of the hash: keys with nothing in their low bits want a hash that mixes ([concurrent::map](../map/README.md)). With `is_transparent` in both `Hash` and `KeyEqual` (as `std::hash` and `std::equal_to` of a `string` have it), `get` and `erase` take a key of another type and build no `Key` for the search. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |
| `KeyEqual` | The equality of the keys, `std::equal_to<Key>` by default. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `size_type` | `size_t` |
| `clock` | `sgcl::clock`: the steady clock, or the manual clock while one is installed |
| `duration` | `clock::duration`, the steady clock's; every `std::chrono::duration` of whole units converts to it |
| `time_point` | `clock::time_point` |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `DefaultSample` | `8` | the number of entries an eviction looks at when the constructor is given none, `static constexpr unsigned` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](cache.md) | constructs an empty cache of a capacity, a time to live and a sample |
| `(destructor)` | leaves the map, its entries and the counters to the collector |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the cache holds an entry |
| [size](size.md) | the number of entries |
| [capacity](capacity.md) | the number of entries the cache keeps |

#### Modifiers

| Function | Description |
|---|---|
| [put](put.md) | inserts or replaces the value of a key, and evicts down to the capacity |
| [erase](erase.md) | erases the entry of a key |
| [clear](clear.md) | erases every entry |

#### Lookup

| Function | Description |
|---|---|
| [get](get.md) | a copy of the value of a key, or nothing |
| [get_or_compute](get_or_compute.md) | the value of a key, computed and put when it is absent or stale |

#### Statistics

| Function | Description |
|---|---|
| [hits](hits.md) | the number of gets that found a value |
| [misses](misses.md) | the number of gets that found none |

#### Observers

| Function | Description |
|---|---|
| [ttl](ttl.md) | the time to live, zero for none |
| [sample_size](sample_size.md) | the number of entries an eviction looks at |
| [hash_function](hash_function.md) | a copy of the hash function |
| [key_eq](key_eq.md) | a copy of the equality of the keys |

## Complexity

- `get`: constant on average, the map's search, then the entry's box and stamp, the tick and the thread's stripe
  ([Benchmarks: The cost of a cache operation](../benchmarks.md#the-cost-of-a-cache-operation): 58 ns a hit on one
  thread over 100,000 entries, 17 ns across four, nothing shared being written).
- `put`: constant on average for the insertion or the replacement; at capacity, plus the eviction, a walk of
  `sample` entries and an erasure (740 ns at capacity on one thread, the same page).
- `get_or_compute`: a `get`; on a miss, the call of `f` and a `put`.
- `erase`: constant on average. `clear`: linear in the number of entries.
- `size`, `capacity`, `ttl`, `sample_size`: constant. `hits`, `misses`: the sum of 16 stripes.

Against an exact LRU (`std::unordered_map` and a `std::list` under a mutex) the cache is twice as slow on one
thread and six times as fast from four threads up
([Benchmarks: The single-producer queue and the cache](../benchmarks.md#the-single-producer-queue-and-the-cache)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Document {
    string name;
    int size;
};

// A cache of documents in front of a slow load, shared by the worker threads through one
// managed object: the hundred most recently read, none older than ten minutes
struct Server {
    concurrent::cache<string, tracked_ptr<Document>> documents{100, std::chrono::minutes(10)};
};

static root_ptr<Server> server = make_tracked<Server>();  // a global: a root

tracked_ptr<Document> load(const string& name) {  // the slow part
    return make_tracked<Document>(name, int(name.size()));
}

int main() {
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([t] {
            for (int i : range(10000)) {
                // fifty documents, read over and over
                string name = "doc" + to_string((i * 7 + t) % 50);
                server->documents.get_or_compute(name, [&] { return load(name); });
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    auto& documents = server->documents;
    println("{} documents cached, {} hits, {} misses", documents.size(), documents.hits(),
            documents.misses());

    if (auto doc = documents.get("doc7")) {  // a literal: no string made for the lookup
        println("{} is {} characters", (*doc)->name, (*doc)->size);
    }
}
```

Sample output:

```text
50 documents cached, 39904 hits, 96 misses
doc7 is 4 characters
```

## See also

- [concurrent::map](../map/README.md): the map underneath and its rules
- [ordered_map](../../core/ordered_map/README.md): the exact LRU cache in two lines for one thread
- [copy_on_write](../copy_on_write/README.md): a value read by every thread and replaced whole rather than looked up by key
- [README: Lock-free containers](../README.md#lock-free-containers), [core: The rules](../../core/README.md#the-rules)
