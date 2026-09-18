# Sgcl::ConcurrentCache

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentCache.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class ConcurrentCache;
}
```

The same class in the `sgcl` interface: [concurrent_cache](../../concurrent/concurrent_cache.md).

`ConcurrentCache<Key, Value, Hash, Equal>` is a key-value cache shared by any number of threads, bounded by a capacity (a number of entries) and, if asked, by a time to live, evicting the entries least recently used: what Guava's `Cache` and Caffeine are in Java and `MemoryCache` is in .NET, and the concurrent counterpart of the LRU cache that [OrderedDictionary](../Containers/OrderedDictionary.md) gives in two lines to one thread. It is built over [ConcurrentDictionary](ConcurrentDictionary.md), so that `TryGet` is the dictionary's wait-free search and nothing else that is shared: no lock, no list to relink, no counter every thread writes. An exact LRU keeps its entries on a list and moves one to the front on every access, which is a write to a shared structure on every hit, the one thing a cache read by many threads cannot afford (Caffeine's lesson, and the reason it buffers its reads), so the order here is approximated, the way Redis approximates it:

- Every entry keeps a stamp of its last access: the value of a clock that the cache's insertions tick, a counter `Set` advances and `TryGet` reads. A hit stores the current tick into its entry, one relaxed store, and only when the tick differs from the one already there, so the line of an entry read over and over stays shared between the cores.
- When an insertion takes the count past the capacity, the thread that inserted evicts by sampling: it walks `sample` entries on from where its last walk ended (a cursor per thread stripe, so that threads evicting at once walk different stretches of the dictionary's list), removes the stale ones on the way, and removes the oldest stamp among the rest; and again, until the count is at the capacity.
- The ticks are exactly as fine as the evictions need. Two entries used between the same two insertions are equally recent; an entry set is as recent as the uses just before it and older than any use after; an entry untouched over the last *n* insertions is *n* ticks old. With the default sample of 8 the entry evicted is older than seven others at least, so what survives is what was used since most of the cache was inserted, which is what LRU is for: a working set read over and over survives a stream of entries read once, and a set of two behaves exactly as the list would (the example). Sampling more approximates the list closer at a longer eviction; `OrderedDictionary` under a mutex is the one to reach for when the order has to be exact.

The interface is a cache's: `TryGet` a copy of the value or `None`, `Set` insert or replace, `GetOrAdd` the value or the one made by a factory and set, `Remove`, `Clear`, `Count`, `Capacity`, `TimeToLive`, `SampleSize`, `Hits` and `Misses`. The value lives in the dictionary's node, copied in by `Set` and never modified there; a `Set` on a key already present puts the new value in a box of its own that the entry points to, so there is no moment of absence and a `TryGet` in flight reads the old value whole, at one load more on the gets of that entry. A value is copied out because the entry may be evicted by another thread the moment after, so the natural payload is a `Ptr`, or a [String](../Core/String.md), one word each.

## Rules

- The cache holds its dictionary, its counters and its cursors by `Ptr`s, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). A cache the threads share is a member of the managed object they share, held by a `RootPtr` when that object is a global ([RootPtr](../Core/RootPtr.md)), as in the example.
- `TryGet` is wait-free and writes nothing shared but the stamp of the entry it hit, when it changed. `Set`, `GetOrAdd` and `Remove` are lock-free: the dictionary's insertion or removal, and the eviction the insertion owes, a walk of `sample` entries and a removal per entry over the capacity.
- `Count()` is exact (the cache's own count, one word) and at most the capacity once the `Set`s have returned; while several threads insert at once it may pass the capacity by the number of them, each on its way to evict. The count and the tick are the two shared writes of a `Set`, on one cache line; a `TryGet` reads that line.
- With a time to live an entry made more than `ttl` ago is absent: a `TryGet` that finds it stale removes it and misses, `GetOrAdd` makes it again, and an eviction pass removes every stale entry it walks past before it looks for the oldest. A `Set` renews the entry: its time runs from the last `Set`, not from the last `TryGet`. The clock (`std::chrono::steady_clock`) is read only when there is a time to live.
- `GetOrAdd(key, factory)` calls the factory when the key is absent or stale, and sets what it returns. Two threads that miss the same key at once both call it; one insertion wins and both return the value that won, so the factory must be a function of the key alone (Guava's `get(key, loader)` blocks the second thread instead; this one blocks nothing, at the price of the second call).
- `Hits()` and `Misses()` count the gets, including the get inside `GetOrAdd` (a get that finds a stale entry is a miss); the counts are striped over cache lines and stay across `Clear()`.
- The value type is copied in on `Set` and out on `TryGet`, so it is copy-constructible; a `Set(key, Value&&)` moves only into the box of a replacement.
- An entry evicted or removed is destroyed by the collector with its node, once nothing holds it: not at the removal, which other threads may be reading it across ([ConcurrentDictionary](ConcurrentDictionary.md) has the rule). The cursors hold the node each stripe's last walk ended at, so an entry removed under a cursor lives on until that stripe's next eviction; `Clear()` lets go of them.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using InnerType = sgcl::concurrent_cache<Key, Value, Hash, Equal>;
using SizeType = size_t;
using Clock = std::chrono::steady_clock;
using Duration = Clock::duration;
static constexpr unsigned DefaultSample = 8;
```

### Constructors

```cpp
explicit ConcurrentCache(SizeType capacity, Duration ttl = Duration::zero(), unsigned sample = DefaultSample);
ConcurrentCache(const ConcurrentCache&) = delete;
```

A cache of `capacity` entries with no time to live (`ttl` zero) or one, looking at `sample` entries per eviction; the dictionary underneath gets its buckets for the capacity up front. A capacity of 0 keeps nothing: every `Set` inserts and evicts. Any `std::chrono::duration` converts to `Duration`.

```cpp
using namespace std::chrono_literals;
struct Session {};
ConcurrentCache<int, Ptr<Session>> sessions(10000);        // ten thousand, no expiry
ConcurrentCache<String, String> tokens(1000, 5min);        // a thousand, none older than five minutes
ConcurrentCache<String, String> exact(100, {}, 16);        // sixteen entries looked at per eviction
```

### TryGet

```cpp
template<class K = Key> Optional<Value> TryGet(const K& key);
```

A copy of the value under `key`, or `None`: absent, or older than the time to live, in which case the entry is removed. Wait-free: the dictionary's search, a load of the entry's box, a relaxed store of the tick into the entry when it changed; a hit or a miss counted on the thread's stripe. A `K` other than the key type looks up without building a key when `Hash` and `Equal` declare `is_transparent`, as they do for a [String](../Core/String.md): a `std::string_view` or a literal finds a `String` key and no `String` is made for the search.

```cpp
struct Session { void Touch() {} };
ConcurrentCache<int, Ptr<Session>> sessions(10000);
ConcurrentCache<String, String> tokens(1000);
int id = 1;
String header = "bearer 7f3a";
if (auto session = sessions.TryGet(id)) {                  // a Ptr<Session>, held by the Optional
    (*session)->Touch();
}
Optional<String> t = tokens.TryGet(header.View(7));   // a view of the header: no String made for the lookup
```

### Set

```cpp
void Set(const Key& key, const Value& value);
void Set(const Key& key, Value&& value);
```

Inserts a copy of the value under `key`, or replaces the value there (in a box the entry points to from then on: no moment of absence, a `TryGet` in flight reads the old value whole); then, if the count is past the capacity, evicts down to it, one pass of `sample` entries per entry over. The value is copied into a new node so that an insertion lost to another thread's of the same key (the node built, then found taken, and dropped) loses nothing; a replacement moves. A `Set` renews the time to live of the entry.

```cpp
sessions.Set(id, Make<Session>());                         // inserted, or the value replaced
```

### GetOrAdd

```cpp
template<class F> Value GetOrAdd(const Key& key, F&& factory);
```

The value under `key`, or, when it is absent or stale, `factory()` called, set under the key and returned: C#'s `GetOrAdd` with a value factory. Two threads that miss the same key at once both call the factory; one insertion wins and both return its value, so the factory is a function of the key alone, called once or more, and never while anything is locked. The miss and the hit inside are counted.

```cpp
struct Document {};
ConcurrentCache<String, Ptr<Document>> documents(100);
String name = "readme";
Ptr doc = documents.GetOrAdd(name, [&] { return Make<Document>(); });   // made once, then a hit
```

### Remove, Clear

```cpp
template<class K = Key> bool Remove(const K& key);
void Clear();
```

Removes the entry under `key` (whether there was one), or every entry there is at the time of the walk; `Clear` keeps the counts of the hits and the misses. The entry lives on while another thread reads it, and dies with its node.

### Count, IsEmpty, Capacity, TimeToLive, SampleSize, Hits, Misses

```cpp
SizeType Count() const noexcept;
bool IsEmpty() const noexcept;
SizeType Capacity() const noexcept;
Duration TimeToLive() const noexcept;
unsigned SampleSize() const noexcept;
uint64_t Hits() const noexcept;
uint64_t Misses() const noexcept;
```

The number of entries (exact; at most the capacity once the `Set`s have returned), the three parameters of the constructor, and the counts of the gets that found a value and of those that did not, the sums of the stripes.

```cpp
ConcurrentCache<int, int> cache(10);
double hitRate = double(cache.Hits()) / double(cache.Hits() + cache.Misses());
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The cache inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A cache of documents in front of a slow load, shared by the worker
// threads through one managed object: a hit is a wait-free lookup, a
// miss loads the document and sets it, and the cache keeps the hundred
// most recently read, none older than ten minutes
struct Document {
    String name;
    int size;
};

struct Server {
    ConcurrentCache<String, Ptr<Document>> documents{100, std::chrono::minutes(10)};
};
static RootPtr<Server> server = Make<Server>();   // a global: a root

Ptr<Document> load(const String& name) {          // the slow part
    return Make<Document>(name, int(name.Length()));
}

int main() {
    List<Thread> threads;
    for (int t : Range(4)) {
        threads.Emplace([t] {
            for (int i : Range(10000)) {
                String name = "doc" + ToString((i * 7 + t) % 50);   // fifty documents, read over and over
                Ptr doc = server->documents.GetOrAdd(name, [&] { return load(name); });
                if (doc->name != name) {
                    return;
                }
            }
        });
    }
    for (auto& th : threads) {
        th.Join();
    }
    std::cout << server->documents.Count() << " documents cached, " << server->documents.Hits() << " hits, " << server->documents.Misses() << " misses\n";
    if (auto doc = server->documents.TryGet("doc7")) {   // a literal: no String made for the lookup
        std::cout << (*doc)->name << " is " << (*doc)->size << " characters\n";
    }

    ConcurrentCache<String, String> small(2);   // the two-line LRU cache of OrderedDictionary, shared
    small.Set("a", "1");
    small.Set("b", "2");
    small.TryGet("a");                          // a is newer than b now
    small.Set("c", "3");                        // full: b, the oldest, goes
    std::cout << (small.TryGet("b") ? "b kept" : "b evicted") << "\n";
    return server->documents.Count() == 50 ? 0 : 1;
}
```

The output of one run (fifty documents are loaded once each; the misses are a few more than fifty when two threads miss the same document at once, and each loads it):

```
50 documents cached, 39950 hits, 50 misses
doc7 is 4 characters
b evicted
```

The cost of a hit and of a `Set` at capacity is measured on the [concurrent_cache](../../concurrent/concurrent_cache.md#cost) page.

## See also

- [ConcurrentDictionary](ConcurrentDictionary.md), the dictionary underneath and its rules
- [OrderedDictionary](../Containers/OrderedDictionary.md), the exact LRU cache in two lines for one thread, and its example, which the one above repeats
- [CopyOnWrite](CopyOnWrite.md), for a value read by every thread and replaced whole rather than looked up by key
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)
