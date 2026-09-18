# Sgcl::ConcurrentSortedDictionary

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentDictionary.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class Key, class Value, class Compare = std::less<Key>>
    class ConcurrentSortedDictionary;
}
```

The same class in the `sgcl` interface: [concurrent_map](../../concurrent/concurrent_map.md).

`ConcurrentSortedDictionary<Key, Value, Compare>` is a lock-free ordered dictionary shared by any number of threads: a skip list with the algorithm of Herlihy and Shavit (*The Art of Multiprocessor Programming*, the lock-free skip list), the structure Java's `ConcurrentSkipListMap` is. The bottom level is a sorted singly linked list holding every element; each node also stands in a random number of the levels above (a quarter of the nodes of one level in the next), so that a search descends from the top level in logarithmic time and finishes on the bottom list, which alone decides what the dictionary holds. A logical deletion is a marker node linked after the deleted one at each of its levels, Java's way of marking a link without stealing a bit from the pointer (which the collector's pointer maps could not follow); the marked node is unlinked by the next search that passes it. No node is reused while a thread holds it, so there is no ABA and no reclamation scheme ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). The interface has the names of a dictionary, restricted to what a lock-free one can offer: `Find`, `TryGet`, `GetOrAdd`, `ContainsKey`, `LowerBound`, `UpperBound`, `Add`, `Emplace`, `Remove`, `Clear`, `Count`, `IsEmpty`, iteration in key order. There is no `operator[]` and no `Set`: an element's value is replaced through the reference the iterator gives, which is the program's own race to manage, as in Java.

A node is one managed object: the bottom link and the element, then the links of its upper levels, as many as its height, so that a search steps through one object per node at any level.

## Rules

- The container holds the head node (a sentinel of the maximum height, no element) by a `Ptr` and the number of levels in use, so it lives where a `Ptr` may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1). Its iterators hold their node by a `Ptr` and live where the dictionary may.
- `Add`, `Emplace`, `GetOrAdd` and `Remove` are lock-free and linearizable: an insertion takes effect at the compare-exchange that links the node into the bottom list, a removal at the one that marks it there. `Find`, `TryGet`, `ContainsKey`, `LowerBound` and `UpperBound` are wait-free and never write. A key given by rvalue is looked up first and moved only into a node of its own; when another thread inserts the same key between that look and the link, the key has been moved into a node that is dropped: the one case where the promise of no move for a key already there does not hold.
- Iteration is weakly consistent, as Java's: an iterator is valid whatever the other threads do, it skips the elements removed since it passed them, and it may or may not see the ones inserted meanwhile. The element an iterator addresses stays alive for as long as the iterator does, removed or not: its key is immutable, its value is what the threads make of it.
- An element is destroyed by the collector with its node, once nothing holds the node: not at the removal, which other threads may be reading it across. This is the one container of the library whose elements outlive their removal ([Containers](../../containers/README.md#containers)); an element that must be released promptly is held by a `Ptr` whose object does its own cleanup, or watched by an [ExpiryQueue](../Containers/ExpiryQueue.md).
- `Count()` counts the elements: linear, and a snapshot of no particular moment when other threads modify the dictionary, as Java's `size` is. `IsEmpty()` is a step from the head.
- A `Ptr` may not address an element or a node ([The rules](../../core/README.md#the-rules), 4); an iterator holds the node, and a reference or a raw pointer to an element is valid while some iterator or the dictionary holds it.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using KeyType = Key;
using ValueType = Value;
using PairType = Pair<const Key, Value>;
using InnerType = sgcl::concurrent_map<Key, Value, Compare>;
using SizeType = size_t;
using Iterator = InnerType::iterator;               // forward, holds its node
using ConstIterator = InnerType::const_iterator;
```

An iterator is a forward iterator holding its node by a `Ptr` word; `*it` is a `PairType&` (a `const PairType&` for a `ConstIterator`), `it->first` the key, `it->second` the value. An `Iterator` converts to a `ConstIterator`.

### Constructors

```cpp
ConcurrentSortedDictionary();
explicit ConcurrentSortedDictionary(const Compare& cmp);
template<std::input_iterator It> ConcurrentSortedDictionary(It first, It last);
ConcurrentSortedDictionary(std::initializer_list<PairType> il);
ConcurrentSortedDictionary(const ConcurrentSortedDictionary&) = delete;
```

An empty dictionary (the head node on the managed heap), or one built at once from a range or a list, sorted and linked without a search ([concurrent_map](../../concurrent/concurrent_map.md#constructors)).

```cpp
struct Session {};
ConcurrentSortedDictionary<int, Ptr<Session>> sessions;
ConcurrentSortedDictionary<String, int, std::greater<String>> byNameDesc;
ConcurrentSortedDictionary<int, int> m = {{1, 10}, {2, 20}};
```

### begin, end, End

```cpp
Iterator begin(ConcurrentSortedDictionary&) noexcept;   ConstIterator begin(const ConcurrentSortedDictionary&) noexcept;   // free functions
Iterator end(ConcurrentSortedDictionary&) noexcept;     ConstIterator end(const ConcurrentSortedDictionary&) noexcept;
Iterator End() noexcept;                                ConstIterator End() const noexcept;
```

The elements in key order, weakly consistent; `end` holds no node.

```cpp
struct Session { void Touch() {} };
ConcurrentSortedDictionary<int, Ptr<Session>> sessions;
for (auto& [id, session] : sessions) {   // a walk while other threads insert and remove
    session->Touch();
}
```

### IsEmpty, Count, KeyCompare

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
Compare KeyCompare() const;
```

### Find, TryGet, ContainsKey

```cpp
Iterator Find(const Key& key) noexcept;
ConstIterator Find(const Key& key) const noexcept;
Optional<Value> TryGet(const Key& key) const;
bool ContainsKey(const Key& key) const noexcept;
template<class K = Key> Iterator Find(const K& key) noexcept;   // and const, TryGet, ContainsKey, LowerBound, UpperBound, Remove: a K other than the key type when Compare is transparent
```

The element under `key` as an iterator that holds its node, or `End()`; `TryGet` a copy of the value, or `None`. Wait-free. A `K` other than the key type looks up without building a key when `Compare` declares `is_transparent`, as `std::less` does for a [String](../Core/String.md): a `std::string_view` or a literal finds a `String` key and no `String` is made for the search.

```cpp
struct Session { void Touch() {} };
ConcurrentSortedDictionary<int, Ptr<Session>> sessions;
if (auto it = sessions.Find(42); it != sessions.End()) {
    it->second->Touch();                 // the node is held by `it` for as long as it exists
}
```

### LowerBound, UpperBound

```cpp
Iterator LowerBound(const Key& key) noexcept;
ConstIterator LowerBound(const Key& key) const noexcept;
Iterator UpperBound(const Key& key) noexcept;
ConstIterator UpperBound(const Key& key) const noexcept;
```

The first element whose key is not less than `key`, and the first whose key is greater; wait-free.

```cpp
ConcurrentSortedDictionary<int, int> m = {{5, 5}, {15, 15}, {25, 25}};
for (auto it = m.LowerBound(10); it != m.End() && it->first < 20; ++it) {   // the keys in [10, 20)
    std::cout << it->first << "\n";
}
```

### Add, Emplace, GetOrAdd

```cpp
bool Add(const Key& key, const Value& value);
bool Add(const Key& key, Value&& value);
template<class... A> bool Emplace(const Key& key, A&&... a);
template<class... A> Iterator GetOrAdd(const Key& key, A&&... a);
```

Adds an element unless its key is taken: `true`, or `false` for the one already there. `Emplace` looks the key up first and builds the value from `a...` only when the key is new; `GetOrAdd` does the same and returns the entry either way, new or existing, as an iterator that holds its node. A concurrent insertion of the same key wins or loses at the compare-exchange on the bottom list: exactly one of them adds.

```cpp
struct Session {};
ConcurrentSortedDictionary<int, Ptr<Session>> sessions;
if (!sessions.Add(42, Make<Session>())) {
    // another thread's session under 42: sessions.Find(42)->second
}
auto it = sessions.GetOrAdd(42, Make<Session>());   // the existing one
```

### Remove, Clear

```cpp
bool Remove(const Key& key);
Iterator Remove(ConstIterator pos);
void Clear();
```

Removes the element under `key` (whether there was one), or the element `pos` addresses if it is still there (the next element in key order is returned). The node is marked at each of its levels, the bottom one last, which is where one thread wins when several remove the same element, then unlinked by a search; the element itself lives on for as long as some iterator holds its node, and dies with the node at a sweep. `Clear` removes every element there is at the time of the walk.

```cpp
struct Session { bool Expired() const { return true; } };
ConcurrentSortedDictionary<int, Ptr<Session>> sessions;
sessions.Add(42, Make<Session>());
sessions.Remove(42);
for (auto it = begin(sessions); it != end(sessions);) {
    it = it->second->Expired() ? sessions.Remove(it) : std::next(it);
}
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The skip list inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A registry shared by writers and readers: writers add and remove,
// readers look up and walk, nobody locks and nobody frees
struct Entry {
    int id;
    int hits = 0;
};

int main() {
    ConcurrentSortedDictionary<int, Ptr<Entry>> registry;   // could as well be in a managed object
    Atomic<long> found = 0;
    List<Thread> threads;
    for (int w : Range(4)) {
        threads.Emplace([&, w] {
            for (int i : Range(250)) {
                int id = i * 4 + w;                              // 1000 keys between the four writers
                registry.Emplace(id, Make<Entry>(id));
            }
            for (int id = w; id < 1000; id += 8) {
                registry.Remove(id);                             // half of this writer's keys taken out again
            }
        });
        threads.Emplace([&] {
            for (int i : Range(10000)) {
                if (auto it = registry.Find(i % 1000); it != registry.End()) {
                    ++it->second->hits;                          // the entry lives while `it` does, removed or not
                    ++found;
                }
            }
            long walked = 0;
            for (auto& [id, entry] : registry) {                 // weakly consistent: sorted, live at the time
                walked += entry->id == id;
            }
            found += walked > 0;
        });
    }
    for (auto& t : threads) {
        t.Join();
    }
    std::cout << registry.Count() << " entries, " << found << " lookups hit\n";
    return registry.Count() == 500 ? 0 : 1;
}
```

The output of one run (the hits depend on how the threads interleave):

```
500 entries, 19702 lookups hit
```

## See also

- [ConcurrentQueue](ConcurrentQueue.md), [ConcurrentStack](ConcurrentStack.md) for the lock-free sequences; [ConcurrentDictionary](ConcurrentDictionary.md) for the hash table
- [SortedDictionary](../Containers/SortedDictionary.md), the sequential dictionary with the full interface
- [Atomic](Atomic.md), what every link is; [ExpiryQueue](../Containers/ExpiryQueue.md) for a cleanup when a removed element's node dies
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)
