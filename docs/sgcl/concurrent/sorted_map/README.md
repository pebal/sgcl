[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>

```cpp
#include "sgcl/concurrent/sorted_map.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class Key, class T, class Compare = std::less<Key>>
    class sorted_map;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::sorted_map<Key, T, Compare>` is a lock-free sorted map shared by any number of threads: a skip
list with the algorithm of Herlihy and Shavit (*The Art of Multiprocessor Programming*, the lock-free skip list),
the structure Java's `ConcurrentSkipListMap` is. The bottom level is a sorted singly linked list holding every
element; each node also stands in a random number of the levels above, a quarter of the nodes of one level in the
next, up to 32 levels, so that a search descends from the top level in logarithmic time and finishes on the
bottom list, which alone decides what the map holds. An insertion links its node into the bottom list with a
compare-exchange, then into its upper levels.

A logical deletion is a marker node linked after the deleted one at each of its levels, Java's way of marking a
link without stealing a bit from the pointer, which the collector's pointer maps could not follow; the marked node
is unlinked by the next insertion or erasure whose search passes it, while a lookup steps over it without writing.
No node is reused while a thread holds it, so there is no ABA and no reclamation scheme
([README: Lock-free containers](../README.md#lock-free-containers)). A node is one managed object: the bottom link
and the element, then the links of its upper levels, as many as its height, so that a search steps through one
object per node at any level.

What differs from `std::map`: the interface has its names, restricted to what a lock-free map can offer. There is
no `operator[]`, no `at`, no `insert_or_assign` and no node handle; the iterators are forward iterators, valid
whatever the other threads do; `size()` counts in linear time; and an erased element is destroyed by the
collector, once nothing holds its node, not at the erase. A mapped value is replaced through the reference an
iterator gives, which is the program's own race to manage, as in Java. What differs from Java's
`ConcurrentSkipListMap`: `try_emplace` is its `putIfAbsent`, `lower_bound` and `upper_bound` its `ceilingEntry`
and `higherEntry`, and an iterator gives the element itself, not a copy of the entry. Go's library has no sorted
map for many goroutines; its `sync.Map` is a hash map.

## Rules

- The container holds the head node, a sentinel of the maximum height and no element, by a `tracked_ptr`, and the number
  of levels in use. Its iterators hold their node by a `tracked_ptr`.
- Every member function may be called from any thread at any time. `insert`, `emplace`, `try_emplace`, `erase` and
  `clear` are lock-free and linearizable: an insertion takes effect at the compare-exchange that links the node
  into the bottom list, an erasure at the one that marks it there. `find`, `contains`, `count`, `lower_bound`,
  `upper_bound` and `value_or` are wait-free and never write. Nothing waits.
- Iteration is weakly consistent, as Java's: an iterator is valid whatever the other threads do, it skips the
  elements erased since it passed them, and it may or may not see the ones inserted meanwhile. The element an
  iterator addresses stays alive for as long as the iterator does, erased or not: its key is immutable, its mapped
  value is what the threads make of it.
- An element is destroyed by the collector with its node, once nothing holds the node: not at the erase, which
  other threads may be reading it across. The maps and sets of this module are the containers of the library
  whose elements outlive their erasure ([Containers](../../core/README.md#containers)); an element that must be
  released promptly is held by a `tracked_ptr` whose object does its own cleanup, or watched by an
  [expiry_queue](../../core/expiry_queue/README.md).
- A `tracked_ptr` may not address an element or a node ([The rules](../../core/README.md#the-rules), 4); an iterator
  holds the node, and a reference or a raw pointer to an element is valid while an iterator holds its node or the
  element is in the map.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys, ordered by `Compare`. A key in the map is never changed. |
| `T` | The type of the mapped values. `value_or` requires it to be copyable. |
| `Compare` | A strict weak ordering of the keys, `std::less<Key>` by default. When it declares `is_transparent` (as `std::less` of a [string](../../core/string/README.md) does), the lookups and `erase` take a key of another type. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |
| `value_type` | `pair<const Key, T>` ([aliases](../../core/aliases.md)) |
| `key_compare` | `Compare` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `iterator` | a forward iterator over `value_type`, holding its node by a `tracked_ptr`; `*it` is a `value_type&`, `it->first` the key, `it->second` the mapped value |
| `const_iterator` | the same over `const value_type`; an `iterator` converts to it |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_map.md) | constructs an empty map, or one built at once from a range or a list |
| `(destructor)` | leaves the nodes, and the elements in them, to the collector |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first element |
| [end, cend](end.md) | an iterator past the last element |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the map holds an element |
| [size](size.md) | counts the elements |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every element there is |
| [insert](insert.md) | inserts an element, or the elements of a range, unless the key is taken |
| [emplace](emplace.md) | constructs an element in a node of its own and inserts it unless the key is taken |
| [try_emplace](try_emplace.md) | constructs and inserts an element only when the key is absent, with one search |
| [erase](erase.md) | erases the element under a key, or the one an iterator addresses |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements under a key, 0 or 1 |
| [find](find.md) | finds the element under a key |
| [contains](contains.md) | checks whether the map holds a key |
| [lower_bound](lower_bound.md) | the first element whose key is not less than a key |
| [upper_bound](upper_bound.md) | the first element whose key is greater than a key |
| [value_or](value_or.md) | a copy of the value under a key, or a fallback |

#### Observers

| Function | Description |
|---|---|
| [key_comp](key_comp.md) | the comparison of the keys |

## Complexity

- `find`, `contains`, `count`, `lower_bound`, `upper_bound`, `value_or`: logarithmic in the size, expected (the
  heights are random).
- `insert`, `emplace`, `try_emplace`, `erase`: logarithmic in the size, expected, plus a search again after each
  compare-exchange lost to another thread.
- `begin`, `empty`, an iterator's `++`: constant, plus the erased nodes not unlinked yet. `size`, `clear`: linear.
- The constructors from a range or a list: the sort of the elements, then a store per link and no search; 110 to
  200 ns per element for 200,000 random keys, against 380 to 600 by the inserts.

A node is allocated per element and a marker per level of an erased node. A search loads some thirty links over
200,000 keys, each through a hazard pointer: 322 ns per lookup on one thread against 197 for `std::map` under an
uncontended mutex, and from four threads up faster than `std::map` under either lock on every operation (27 ns
per lookup at sixteen threads against 237;
[Benchmarks: Concurrent containers](../benchmarks.md#concurrent-containers)).

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all, in any thread | never |

An iterator holds its node by a `tracked_ptr`: the node, and the element in it, live for as long as the iterator
does. After the element is erased the iterator still reads it, and `++` steps to the next element not erased.
`end()` holds no node.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A registry shared by writers and readers: writers insert and erase,
// readers look up and walk, nobody locks and nobody frees
struct Entry {
    int id;
    atomic<int> hits = 0;
};

int main() {
    concurrent::sorted_map<int, tracked_ptr<Entry>> registry;
    atomic<long> found = 0, out_of_order = 0;
    vector<thread> threads;
    for (int w : range(4)) {
        threads.emplace_back([&, w] {
            for (int i : range(250)) {
                int id = i * 4 + w;  // 1000 keys between the four writers
                registry.try_emplace(id, make_tracked<Entry>(id));
            }
            for (int i : range(125)) {
                registry.erase(i * 8 + w);  // half of this writer's keys taken out again
            }
        });
        threads.emplace_back([&] {
            for (int i : range(10000)) {
                if (auto it = registry.find(i % 1000); it != registry.end()) {
                    ++it->second->hits;  // the entry lives while `it` does, erased or not
                    ++found;
                }
            }
            int last = -1;
            for (auto& [id, entry] : registry) {  // weakly consistent, and sorted
                out_of_order += id <= last;
                last = id;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{} entries, {} out of order", registry.size(), out_of_order.load());
    println("{} lookups hit", found.load());
}
```

Sample output:

```text
500 entries, 0 out of order
18438 lookups hit
```

## See also

- [sorted_set](../sorted_set/README.md): the same skip list with the key as the element
- [map](../map/README.md): the lock-free hash map, unordered, a lookup in a few steps
- [sorted_map](../../core/sorted_map/README.md): the sequential map with the whole interface of `std::map`
- [queue](../queue/README.md), [stack](../stack/README.md): the lock-free sequences
- [atomic](../../core/atomic.md): what every link is; [expiry_queue](../../core/expiry_queue/README.md): a cleanup when an
  erased element's node dies
- [README: Lock-free containers](../README.md#lock-free-containers)
