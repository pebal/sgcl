# sgcl::concurrent_cache

```cpp
#include "sgcl/concurrent/concurrent_cache.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class concurrent_cache;
}
```

The same class in the `Sgcl` interface: [ConcurrentCache](../Sgcl/Concurrent/ConcurrentCache.md).

`sgcl::concurrent_cache<Key, T, Hash, KeyEqual>` is a key-value cache shared by any number of threads, bounded by a capacity (a number of entries) and, if asked, by a time to live, evicting the entries least recently used: what Guava's `Cache` and Caffeine are in Java (the standard libraries of Go and Java have none, and everybody writes one), and the concurrent counterpart of the LRU cache that [ordered_map](../containers/ordered_map.md) gives in two lines to one thread. It is built over [concurrent_unordered_map](concurrent_unordered_map.md), so that `get` is the map's wait-free search and nothing else that is shared: no lock, no list to relink, no counter every thread writes. An exact LRU keeps its entries on a list and moves one to the front on every access, which is a write to a shared structure on every hit, the one thing a cache read by many threads cannot afford (Caffeine's lesson, and the reason it buffers its reads), so the order here is approximated, the way Redis approximates it:

- Every entry keeps a stamp of its last access: the value of a clock that the cache's insertions tick, a counter `put` advances and `get` reads. A hit stores the current tick into its entry, one relaxed store, and only when the tick differs from the one already there, so the line of an entry read over and over stays shared between the cores.
- When an insertion takes the size past the capacity, the thread that inserted evicts by sampling: it walks `sample` entries on from where its last walk ended (a cursor per thread stripe, so that threads evicting at once walk different stretches of the map's list), erases the stale ones on the way, and erases the oldest stamp among the rest; and again, until the size is at the capacity.
- The ticks are exactly as fine as the evictions need. Two entries used between the same two insertions are equally recent; an entry put is as recent as the uses just before it and older than any use after; an entry untouched over the last *n* insertions is *n* ticks old. With the default sample of 8 the entry evicted is older than seven others at least, so what survives is what was used since most of the cache was inserted, which is what LRU is for: a working set read over and over survives a stream of entries read once, and a set of two behaves exactly as the list would (the example). Sampling more approximates the list closer at a longer eviction (16 entries looked at instead of 8); `ordered_map` under a mutex is the one to reach for when the order has to be exact.

The interface is a cache's: `get` a copy of the value or nothing, `put` insert or replace, `get_or_compute` the value or the one computed and put, `erase`, `clear`, `size`, `capacity`, `ttl`, `sample_size`, `hits` and `misses`. The value lives in the map's node, copied in by `put` and never modified there; a `put` on a key already present puts the new value in a box of its own that the entry points to, so there is no moment of absence and a `get` in flight reads the old value whole, at one load more on the gets of that entry. A value is copied out because the entry may be evicted by another thread the moment after, so the natural payload is a `tracked_ptr`, or a [string](../core/string.md), one word each.

## Rules

- The cache holds its map, its counters and its cursors by `tracked_ptr`s, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). A cache the threads share is a member of the managed object they share, held by a `root_ptr` when that object is a global ([root_ptr](../core/root_ptr.md)), as in the example.
- `get` is wait-free and writes nothing shared but the stamp of the entry it hit, when it changed. `put`, `get_or_compute` and `erase` are lock-free: the map's `try_emplace` or `erase`, and the eviction the insertion owes, a walk of `sample` entries and an erasure per entry over the capacity.
- `size()` is exact (the cache's own count, one word) and at most the capacity once the `put`s have returned; while several threads insert at once it may pass the capacity by the number of them, each on its way to evict. The count of the entries is one word every insertion and erasure writes, the tick another: the two shared writes of a `put`, on one cache line; a `get` reads that line.
- With a time to live an entry made more than `ttl` ago is absent: a `get` that finds it stale erases it and misses, `get_or_compute` computes it again, and an eviction pass erases every stale entry it walks past before it looks for the oldest. A `put` renews the entry: its time runs from the last `put`, not from the last `get`. The clock (`steady_clock`) is read by `get`, `put` and the evictions only when there is a time to live.
- `get_or_compute(key, f)` calls `f` when the key is absent or stale, and puts what it returns. Two threads that miss the same key at once both call `f`; one insertion wins and both return the value that won, so `f` must be a function of the key alone (Guava's `get(key, loader)` blocks the second thread instead; this one blocks nothing, at the price of the second computation).
- `hits()` and `misses()` count the gets, including the get inside `get_or_compute` (a get that finds a stale entry is a miss); the counts are striped over cache lines, as the map's count is, and stay across `clear()`.
- The value type is copied in on `put` and out on `get`, so it is copy-constructible; a `put(key, T&&)` moves only into the box of a replacement.
- An entry evicted or erased is destroyed by the collector with its node, once nothing holds it: not at the erasure, which other threads may be reading it across ([concurrent_unordered_map](concurrent_unordered_map.md) has the rule). The cursors hold the node each stripe's last walk ended at, so an entry erased under a cursor lives on until that stripe's next eviction; `clear()` lets go of them.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using key_type = Key;
using mapped_type = T;
using hasher = Hash;
using key_equal = KeyEqual;
using size_type = size_t;
using clock = std::chrono::steady_clock;
using duration = clock::duration;
using time_point = clock::time_point;
static constexpr unsigned DefaultSample = 8;
```

### Constructors

```cpp
explicit concurrent_cache(size_type capacity, duration ttl = duration::zero(), unsigned sample = DefaultSample, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual());
concurrent_cache(const concurrent_cache&) = delete;
```

A cache of `capacity` entries with no time to live (`ttl` zero) or one, looking at `sample` entries per eviction; the map underneath gets its buckets for the capacity up front. A capacity of 0 keeps nothing: every `put` inserts and evicts. Any `std::chrono::duration` converts to `duration`.

```cpp
using namespace std::chrono_literals;
sgcl::concurrent_cache<int, sgcl::tracked_ptr<Session>> sessions(10000);        // ten thousand, no expiry
sgcl::concurrent_cache<sgcl::string, sgcl::string> tokens(1000, 5min);           // a thousand, none older than five minutes
sgcl::concurrent_cache<sgcl::string, sgcl::string> exact(100, {}, 16);           // sixteen entries looked at per eviction
```

### get

```cpp
optional<T> get(const Key& key);
template<class K> optional<T> get(const K& key);   // when Hash and KeyEqual are transparent
```

A copy of the value under `key`, or `nullopt`: absent, or older than the time to live, in which case the entry is erased. Wait-free: the map's search, a load of the entry's box, a relaxed store of the tick into the entry when it changed; a hit or a miss counted on the thread's stripe. With a transparent hash and equality (`is_transparent`, as `std::hash` and `std::equal_to` of a [string](../core/string.md) are) `get` takes a key of another type and builds none: a `string_view` or a literal finds a `string` key with no string made for the search.

```cpp
if (auto session = sessions.get(id)) {                     // a tracked_ptr<Session>, held by the optional
    (*session)->touch();
}
sgcl::string header = "bearer 7f3a";
sgcl::optional<sgcl::string> t = tokens.get(header.view(7));   // a view of the header: no string made for the lookup
```

### put

```cpp
void put(const Key& key, const T& value);
void put(const Key& key, T&& value);
```

Inserts a copy of the value under `key`, or replaces the value there (in a box the entry points to from then on: no moment of absence, a `get` in flight reads the old value whole); then, if the size is past the capacity, evicts down to it, one pass of `sample` entries per entry over. The value is copied into a new node so that an insertion lost to another thread's of the same key (the node built, then found taken, and dropped) loses nothing; a replacement moves. A `put` renews the time to live of the entry.

```cpp
sessions.put(id, sgcl::make_tracked<Session>(id));         // inserted, or the value replaced
```

### get_or_compute

```cpp
template<class F> T get_or_compute(const Key& key, F&& f);
```

The value under `key`, or, when it is absent or stale, `f()` computed, put under the key and returned. Two threads that miss the same key at once both compute; one insertion wins and both return its value, so `f` is a function of the key alone, called once or more, and never while anything is locked. The miss and the hit inside are counted.

```cpp
sgcl::tracked_ptr doc = documents.get_or_compute(name, [&] { return load(name); });   // loaded once, then a hit
```

### erase, clear

```cpp
bool erase(const Key& key);
template<class K> bool erase(const K& key);   // when Hash and KeyEqual are transparent
void clear();
```

Erases the entry under `key` (whether there was one), or every entry there is at the time of the walk; `clear` keeps the counts of the hits and the misses. The entry lives on while another thread reads it, and dies with its node.

### size, empty, capacity, ttl, sample_size, hits, misses, hash_function, key_eq

```cpp
size_type size() const noexcept;
bool empty() const noexcept;
size_type capacity() const noexcept;
duration ttl() const noexcept;
unsigned sample_size() const noexcept;
uint64_t hits() const noexcept;
uint64_t misses() const noexcept;
hasher hash_function() const;
key_equal key_eq() const;
```

The number of entries (exact; at most the capacity once the `put`s have returned), the three parameters of the constructor, and the counts of the gets that found a value and of those that did not, the sums of the stripes.

```cpp
double hit_rate = double(cache.hits()) / double(cache.hits() + cache.misses());
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A cache of documents in front of a slow load, shared by the worker
// threads through one managed object: a hit is a wait-free lookup, a
// miss loads the document and puts it, and the cache keeps the hundred
// most recently read, none older than ten minutes
struct Document {
    sgcl::string name;
    int size;
};

struct Server {
    sgcl::concurrent_cache<sgcl::string, sgcl::tracked_ptr<Document>> documents{100, std::chrono::minutes(10)};
};
static sgcl::root_ptr<Server> server = sgcl::make_tracked<Server>();   // a global: a root

sgcl::tracked_ptr<Document> load(const sgcl::string& name) {           // the slow part
    return sgcl::make_tracked<Document>(name, int(name.size()));
}

int main() {
    sgcl::vector<sgcl::thread> threads;
    for (int t : sgcl::range(4)) {
        threads.emplace_back([t] {
            for (int i : sgcl::range(10000)) {
                sgcl::string name = "doc" + sgcl::to_string((i * 7 + t) % 50);   // fifty documents, read over and over
                sgcl::tracked_ptr doc = server->documents.get_or_compute(name, [&] { return load(name); });
                if (doc->name != name) {
                    return;
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    std::cout << server->documents.size() << " documents cached, " << server->documents.hits() << " hits, " << server->documents.misses() << " misses\n";
    if (auto doc = server->documents.get("doc7")) {   // a literal: no string made for the lookup
        std::cout << (*doc)->name << " is " << (*doc)->size << " characters\n";
    }

    sgcl::concurrent_cache<sgcl::string, sgcl::string> small(2);   // the two-line LRU cache of ordered_map, shared
    small.put("a", "1");
    small.put("b", "2");
    small.get("a");                                                // a is newer than b now
    small.put("c", "3");                                           // full: b, the oldest, goes
    std::cout << (small.get("b") ? "b kept" : "b evicted") << "\n";
    return server->documents.size() == 50 ? 0 : 1;
}
```

The output of one run (fifty documents are loaded once each; the misses are a few more than fifty when two threads miss the same document at once, and each loads it):

```
50 documents cached, 39943 hits, 57 misses
doc7 is 4 characters
b evicted
```

## Cost

A cache of 100,000 entries keyed by `long` with `tracked_ptr` values, random keys, in nanoseconds per operation (an Apple M-series machine, `-O2`, the best of three runs on a machine that was not idle, so an upper bound):

| operation | 1 thread | 4 threads (across the threads) |
|---|---|---|
| `get`, a hit | 58 | 17 |
| `get`, a hit, with a time to live | 87 | |
| `put` of a new key at capacity (an eviction each) | 740 | 650 |

The map's own `find` over the same entries is 40 ns: a hit is the search, then the entry's box (null) and stamp on the node's line, the tick, and the thread's stripe; four threads hitting cost a quarter of one, nothing shared being written. A `put` at capacity is an insertion (a node made and linked), the walk of eight entries, each a node of the list and mostly a cache miss over 100,000 of them, an erasure (a marker made, the node unlinked by a search), and a cursor made: the map's own insertion and erasure of a key are 200 ns of it, the sample most of the rest (650 ns at a sample of 5, 1000 at 16). Four threads putting at once share the tick and the count, one line, and evict on stretches of their own. With a time to live a hit reads the clock.

## See also

- [concurrent_unordered_map](concurrent_unordered_map.md), the map underneath and its rules
- [ordered_map](../containers/ordered_map.md), the exact LRU cache in two lines for one thread, and its example, which the one above repeats
- [copy_on_write](copy_on_write.md), for a value read by every thread and replaced whole rather than looked up by key
- [Benchmarks](benchmarks.md#the-single-producer-queue-the-cache-and-the-persistent-map): against the exact LRU under a mutex, one to sixteen threads
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)
