[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>

```cpp
#include "sgcl/concurrent/sorted_set.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class Key, class Compare = std::less<Key>>
    class sorted_set;
}
```

`sgcl::concurrent::sorted_set<Key, Compare>` is a lock-free sorted set shared by any number of threads: the skip
list of [sorted_map](../sorted_map/README.md) with the key as the element, the structure Java's `ConcurrentSkipListSet` is.
The bottom level is a sorted singly linked list holding every key; each node also stands in a random number of the
levels above, a quarter of the nodes of one level in the next, so that a search descends from the top level in
logarithmic time and finishes on the bottom list, which alone decides what the set holds. An erased node is marked
by a marker node linked after it at each of its levels, and unlinked by the next insertion or erasure whose search
passes it; no node is reused while a thread holds it, so there is no ABA and no reclamation scheme
([README: Lock-free containers](../README.md#lock-free-containers)).

What differs from `std::set`: the interface has its names, with forward iterators valid whatever the other threads
do, a `size()` that counts in linear time, and keys destroyed by the collector once nothing holds their node, not
at the erase. The keys are const, as in `std::set`: `iterator` and `const_iterator` are one type. `insert` of a key
searches once and builds the node only when the key is absent. Go's library has no sorted set for many
goroutines.

## Rules

- The container holds the head node, a sentinel of the maximum height and no key, by a `tracked_ptr`, and the
  number of levels in use, so it lives where a `tracked_ptr` may: on a thread's stack or inside a managed object
  ([The rules](../../core/README.md#the-rules), 1). Its iterators hold their node by a `tracked_ptr` and live where
  the set may.
- Every member function may be called from any thread at any time. `insert`, `emplace`, `erase` and `clear` are
  lock-free and linearizable: an insertion takes effect at the compare-exchange that links the node into the
  bottom list, an erasure at the one that marks it there. `find`, `contains`, `count`, `lower_bound` and
  `upper_bound` are wait-free and never write. Nothing waits.
- Iteration is weakly consistent, as Java's: an iterator is valid whatever the other threads do, it skips the keys
  erased since it passed them, and it may or may not see the ones inserted meanwhile. The key an iterator
  addresses stays alive for as long as the iterator does, erased or not.
- A key is destroyed by the collector with its node, once nothing holds the node: not at the erase, which other
  threads may be reading it across ([Containers](../../core/README.md#containers)).
- A `tracked_ptr` may not address a key or a node ([The rules](../../core/README.md#the-rules), 4); an iterator holds
  the node, and a reference or a raw pointer to a key is valid while an iterator holds its node or the key is in
  the set.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys, the elements of the set, ordered by `Compare`. A key in the set is never changed. |
| `Compare` | A strict weak ordering of the keys, `std::less<Key>` by default. When it declares `is_transparent` (as `std::less` of a [string](../../core/string/README.md) does), the lookups and `erase` take a key of another type. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `value_type` | `Key` |
| `key_compare` | `Compare` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `iterator` | a forward iterator over `const Key`, holding its node by a `tracked_ptr` |
| `const_iterator` | the same type as `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_set.md) | constructs an empty set, or one built at once from a range or a list |
| `(destructor)` | leaves the nodes, and the keys in them, to the collector |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first key |
| [end, cend](end.md) | an iterator past the last key |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the set holds a key |
| [size](size.md) | counts the keys |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every key there is |
| [insert](insert.md) | inserts a key, with one search, or the keys of a range |
| [emplace](emplace.md) | constructs a key in a node of its own and inserts it unless the set holds it |
| [erase](erase.md) | erases a key, or the one an iterator addresses |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of keys equivalent to a key, 0 or 1 |
| [find](find.md) | finds a key |
| [contains](contains.md) | checks whether the set holds a key |
| [lower_bound](lower_bound.md) | the first key not less than a key |
| [upper_bound](upper_bound.md) | the first key greater than a key |

#### Observers

| Function | Description |
|---|---|
| [key_comp](key_comp.md) | the comparison of the keys |

## Complexity

- `find`, `contains`, `count`, `lower_bound`, `upper_bound`: logarithmic in the size, expected (the heights are
  random).
- `insert`, `emplace`, `erase`: logarithmic in the size, expected, plus a search again after each compare-exchange
  lost to another thread.
- `begin`, `empty`, an iterator's `++`: constant, plus the erased nodes not unlinked yet. `size`, `clear`: linear.
- The constructors from a range or a list: the sort of the keys, then a store per link and no search.

A node is allocated per key and a marker per level of an erased node. The set measures as the map does: 314 ns per
lookup on one thread, 26 at sixteen threads against 219 for `std::set` under a mutex
([Benchmarks: Concurrent containers](../benchmarks.md#concurrent-containers)).

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all, in any thread | never |

An iterator holds its node by a `tracked_ptr`: the node, and the key in it, live for as long as the iterator does.
After the key is erased the iterator still reads it, and `++` steps to the next key not erased. `end()` holds no
node.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// Timestamps from many threads, read back in order while they arrive
int main() {
    concurrent::sorted_set<long> stamps;
    vector<thread> threads;
    for (int t : range(4)) {
        threads.emplace_back([&, t] {
            for (long i : range(1000L)) {
                stamps.insert(i * 4 + t);
            }
        });
    }
    long walks = 0, disorder = 0, last = -1;
    while (last < 3999) {  // walks while the writers are at it, until one sees the greatest stamp
        last = -1;
        for (long s : stamps) {  // sorted at every walk, whatever the writers are doing
            disorder += s <= last;
            last = s;
        }
        ++walks;
    }
    for (auto& th : threads) {
        th.join();
    }
    println("{} stamps, {} out of order; from 1000: {}", stamps.size(), disorder,
            *stamps.lower_bound(1000));
    println("{} walks", walks);
}
```

Sample output:

```text
4000 stamps, 0 out of order; from 1000: 1000
1034 walks
```

## See also

- [sorted_map](../sorted_map/README.md): the same skip list with a mapped value
- [set](../set/README.md): the lock-free hash set, unordered, a lookup in a few steps
- [sorted_set](../../core/sorted_set/README.md): the sequential set with the whole interface of `std::set`
- [README: Lock-free containers](../README.md#lock-free-containers)
