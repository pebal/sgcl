# Sgcl::ConcurrentDictionary

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>>
    class ConcurrentDictionary;
}
```

The same class in the `sgcl` interface: [concurrent_unordered_map](../../concurrent/concurrent_unordered_map.md).

`ConcurrentDictionary<Key, Value, Hash, Equal>` is a lock-free hash dictionary shared by any number of threads: the split-ordered list of Shalev and Shavit (*Split-Ordered Lists: Lock-Free Extensible Hash Tables*, 2006), the structure that makes a resizable lock-free hash table possible, what Java's `ConcurrentHashMap` and C#'s `ConcurrentDictionary` are for. Every element sits in one sorted singly linked list (Harris and Michael's, with marker nodes for the deletions as in [ConcurrentSortedDictionary](ConcurrentSortedDictionary.md)), ordered by the bit reversal of its hash; an array of buckets points into that list at dummy nodes, one per bucket, made on the bucket's first use and inserted after the parent bucket's dummy (the bucket's number with its highest bit cleared). In split order the elements of bucket *i* of an array of *n* follow its dummy and precede the dummy of the bucket that *i* splits into at *2n*, so doubling the array moves no node: the new array gets the old slots copied and the rest made on use, the list stays what it was, and the old array is garbage once nothing walks it. In C++ without a collector that old array, and every node a walk may be standing on, is exactly what a reclamation scheme has to guard; here it is nothing ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). The interface has the names of a dictionary, restricted to what a lock-free one can offer: `Find`, `TryGet`, `GetOrAdd`, `ContainsKey`, `Add`, `Emplace`, `Remove`, `Clear`, `Count`, `IsEmpty`, `BucketCount`, `Reserve`, iteration. There is no `operator[]`, no `Set` and no iteration of one bucket.

The lookups differ from [Dictionary](../Containers/Dictionary.md)'s because a raw pointer into a node another thread may remove would not be safe: `TryGet` hands back a copy of the value in an `Optional`, right whatever happens to the entry after; `Find` and `GetOrAdd` hand back an iterator, which holds the node (a `Ptr` inside), so `it->second` stays valid for as long as the iterator exists, removed or not. An entry once made keeps its value, which the program changes through an [`Atomic`](Atomic.md) inside it.

## Rules

- The container holds its bucket array, its head node and its counters by `Ptr`s, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). Its iterators hold their node by a `Ptr`.
- `Add`, `Emplace`, `GetOrAdd` and `Remove` are lock-free and linearizable: an insertion takes effect at the compare-exchange that links the node into the list, a removal at the one that marks it. `Find`, `TryGet`, `ContainsKey` are wait-free and never write. A key given by rvalue is looked up first and moved only into a node of its own; when another thread inserts the same key between that look and the link, the key has been moved into a node that is dropped: the one case where the promise of no move for a key already there does not hold.
- The count of the elements is striped over cache lines (Java's `LongAdder`): `Count()` is the sum, a snapshot of no particular moment under concurrent modification, exact once the threads are quiet. The array doubles once the elements outnumber the buckets (a load factor of one), by the insertion that notices; `Reserve(n)` doubles it up front.
- Iteration is weakly consistent, as Java's: an iterator is valid whatever the other threads do, it skips the elements removed since it passed them and may or may not see the ones inserted meanwhile; the order is the list's, the bit reversal of the hashes, and it changes with nothing but the elements. The element an iterator addresses stays alive for as long as the iterator does, removed or not.
- An element is destroyed by the collector with its node, once nothing holds the node: not at the removal, which other threads may be reading it across ([ConcurrentSortedDictionary](ConcurrentSortedDictionary.md) has the same rule).
- The dummies of the buckets used stay for the life of the container, one node each; a bucket initialized by two threads at once during a growth of the array gets two, which the list takes in its stride.
- The bucket is the low bits of the hash, and `std::hash` is the identity for integers and pointers, so keys with nothing in their low bits (multiples of a power of two, page-aligned addresses) crowd into a few buckets whatever the size of the array: give such keys a hash that mixes. Keys whose low bits vary (ids, counters, ordinary pointers) are best left as they are: neighbouring keys keep neighbouring nodes, which a mixing hash would scatter (a find of a sequential key 25 ns against 60 mixed, measured).
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using PairType = Pair<const Key, Value>;
using InnerType = sgcl::concurrent_unordered_map<Key, Value, Hash, Equal>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, holds its node
using ConstIterator = InnerType::const_iterator;
```

### Constructors

```cpp
ConcurrentDictionary();
explicit ConcurrentDictionary(SizeType buckets);
template<std::input_iterator It> ConcurrentDictionary(It first, It last);
ConcurrentDictionary(std::initializer_list<PairType> il);
ConcurrentDictionary(const ConcurrentDictionary&) = delete;
```

An empty dictionary with 16 buckets (`buckets` rounded up to a power of two when given), or one built at once from a range or a list, sorted and linked without a search ([concurrent_unordered_map](../../concurrent/concurrent_unordered_map.md#constructors)).

```cpp
struct Session {};
ConcurrentDictionary<int, Ptr<Session>> sessions;
ConcurrentDictionary<String, int> counts(1 << 16);      // 65536 buckets from the start
```

### begin, end, End, IsEmpty, Count, BucketCount, Reserve, HashFunction, KeyEqual

```cpp
Iterator begin(ConcurrentDictionary&) noexcept;         ConstIterator begin(const ConcurrentDictionary&) noexcept;   // free functions
Iterator end(ConcurrentDictionary&) noexcept;           ConstIterator end(const ConcurrentDictionary&) noexcept;
Iterator End() noexcept;                                ConstIterator End() const noexcept;   // what Find returns when absent
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
SizeType BucketCount() const noexcept;
void Reserve(SizeType n);
Hash HashFunction() const;
Equal KeyEqual() const;
```

`IsEmpty` is a step from the head of the list; `Count` the sum of the stripes; `Reserve` grows the array to at least `n` buckets.

### Find, TryGet, ContainsKey

```cpp
Iterator Find(const Key& key) noexcept;
ConstIterator Find(const Key& key) const noexcept;
Optional<Value> TryGet(const Key& key) const;
bool ContainsKey(const Key& key) const noexcept;
template<class K = Key> Iterator Find(const K& key) noexcept;   // and const, TryGet, ContainsKey, Remove: a K other than the key type when Hash and Equal are transparent
```

The element under `key` as an iterator that holds its node, or `End()`; `TryGet` a copy of the value, or `None`. Wait-free: from the bucket's dummy (or the nearest initialized ancestor's) along the list to the element. A `K` other than the key type looks up without building a key when `Hash` and `Equal` declare `is_transparent`, as they do for a [String](../Core/String.md): a `std::string_view` or a literal finds a `String` key and no `String` is made for the search.

```cpp
ConcurrentDictionary<int, int> d = {{1, 10}};
if (auto it = d.Find(1); it != d.End()) {
    int v = it->second;                    // the node held by `it`
}
Optional<int> copy = d.TryGet(1); // 10
bool has = d.ContainsKey(2);               // false
```

### Add, Emplace, GetOrAdd

```cpp
bool Add(const Key& key, const Value& value);
bool Add(const Key& key, Value&& value);
template<class... A> bool Emplace(const Key& key, A&&... a);
template<class... A> Iterator GetOrAdd(const Key& key, A&&... a);
```

Adds an element unless its key is taken: `true`, or `false` for the one already there. `Emplace` looks the key up first and builds the value from `a...` only when the key is new; `GetOrAdd` does the same and returns the entry either way, new or existing, as an iterator that holds its node. A concurrent insertion of the same key wins or loses at the compare-exchange: exactly one adds.

```cpp
struct Session { Atomic<int> hits = 0; };
ConcurrentDictionary<int, Ptr<Session>> sessions;
bool added = sessions.Add(42, Make<Session>());          // true
auto it = sessions.GetOrAdd(42, Make<Session>());        // the existing one; the new Session is garbage
it->second->hits += 1;                                            // changed in place, under the entry
```

### Remove, Clear

```cpp
bool Remove(const Key& key);
Iterator Remove(ConstIterator pos);
void Clear();
```

Removes the element under `key` (whether there was one), or the one `pos` addresses if it is still there (some element after it in the list, or `End()`, is returned); `Clear` removes every element there is at the time of the walk. The node is marked, which is where one thread wins when several remove the same element, then unlinked by a search; the element lives on while an iterator holds its node.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The table inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A word count over many threads: every thread adds or increments,
// nobody locks, the table doubles under them as it fills
struct Count {
    Atomic<int> n = 0;
};

int main() {
    ConcurrentDictionary<String, Ptr<Count>> counts;
    List<Thread> threads;
    for (int t : Range(8)) {
        threads.Emplace([&, t] {
            for (int i : Range(10000)) {
                String word = "w" + ToString((i * 8 + t) % 1000);
                auto it = counts.GetOrAdd(word, Make<Count>());   // one Count per word, whoever gets there first
                ++it->second->n;
            }
        });
    }
    for (auto& th : threads) {
        th.Join();
    }
    int total = 0;
    for (auto& [word, count] : counts) {
        total += count->n;
    }
    std::cout << counts.Count() << " words, " << total << " occurrences, " << counts.BucketCount() << " buckets\n";
    return counts.Count() == 1000 && total == 80000 ? 0 : 1;
}
```

The output of one run (the buckets depend on how the threads interleave):

```
1000 words, 80000 occurrences, 256 buckets
```

## See also

- [ConcurrentHashSet](ConcurrentHashSet.md), the same table with the key as the element
- [ConcurrentSortedDictionary](ConcurrentSortedDictionary.md), the ordered dictionary (a skip list), and the marker nodes both share
- [Dictionary](../Containers/Dictionary.md), the sequential dictionary with the full interface
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)
