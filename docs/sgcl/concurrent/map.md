[sgcl](../README.md) › [concurrent](README.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>

```cpp
#include "sgcl/concurrent/map.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map;
}
```

`sgcl::concurrent::map<Key, T, Hash, KeyEqual>` is a lock-free hash map shared by any number of threads: the
split-ordered list of Shalev and Shavit (*Split-Ordered Lists: Lock-Free Extensible Hash Tables*, 2006), the
structure that makes a resizable lock-free hash table possible, and the one whose resize is the reason for a
collector. Every element sits in one sorted singly linked list (Harris and Michael's, with marker nodes for the
deletions as in [sorted_map](sorted_map.md)), ordered by the bit reversal of its hash; an array of buckets points
into that list at dummy nodes, one per bucket, made on the bucket's first use and inserted after the parent
bucket's dummy (the bucket's number with its highest bit cleared). In split order the elements of bucket *i* of an
array of *n* follow its dummy and precede the dummy of the bucket that *i* splits into at *2n*, so doubling the
array moves no node: the new array gets the old slots copied and the rest made on use, the list stays what it was,
and the old array is garbage once nothing walks it. In C++ without a collector that old array, and every node a
walk may be standing on, is exactly what a reclamation scheme has to guard; here it is nothing
([README: Lock-free containers](README.md#lock-free-containers)).

What differs from `std::unordered_map`: the interface has its names, restricted to what a lock-free map can offer
(`find`, `contains`, `count`, `insert`, `emplace`, `try_emplace`, `erase`, `clear`, `size`, `empty`,
`bucket_count`, `reserve`, iteration, and `value_or` of the library's maps). There is no `operator[]`, no `at`,
no `insert_or_assign`, no node handles and no iteration of one bucket: each would hand out an element by
reference across a moment in which another thread may erase or replace it. An erased element is not destroyed at
the erase but by the collector, and the order of iteration is fixed by the hashes alone. What differs from Java's
`ConcurrentHashMap`: no operation takes a lock, an insertion included; `try_emplace` is its `putIfAbsent`, and
there is no `compute`. What differs from Go's `sync.Map`: the map is typed, and its keys and values are not boxed.

## Rules

- The container holds its bucket array and its head node by `tracked_ptr`s, and its counters by a
  `std::unique_ptr` (plain memory of numbers alone, freed with the map), so it lives where a `tracked_ptr` may: on a
  thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1). Its iterators hold
  their node by a `tracked_ptr` and live where it may.
- Every member function may be called from any thread at any time, concurrently with any other. `find`,
  `contains`, `count` and `value_or` are wait-free once the key's bucket has its dummy node; `insert`, `emplace`,
  `try_emplace` and `erase` are lock-free and linearizable, an insertion at the compare-exchange that links its node
  into the list, an erasure at the one that marks it. Nothing waits but `reserve`, which may spin while another
  thread doubles the array.
- The map synchronizes its structure, not the values: an element is published complete by the link, but a value
  changed in place through an iterator while another thread reads it is a data race. A value that threads change
  after the insertion is an atomic, or a `tracked_ptr` to an object that synchronizes itself.
- The count of the elements is striped over cache lines (Java's `LongAdder`): `size()` is the sum, a snapshot of no
  particular moment under concurrent modification, exact once the threads are quiet. The array doubles once the
  elements outnumber the buckets (a load factor of one), by the insertion that notices; `reserve` doubles it up
  front. The array never shrinks.
- An element is destroyed by the collector with its node, once nothing holds the node: not at the erase, which
  other threads may be reading it across. The element an iterator addresses stays alive as long as the iterator,
  erased or not.
- The dummies of the buckets used stay for the life of the container, one node each; a bucket initialized by two
  threads at once during a growth of the array gets two, which the list takes in its stride.
- The bucket of a key is the low bits of its hash. `std::hash` of an integer is the integer itself (in libc++ and
  libstdc++; of a pointer, the address, in libstdc++), so keys with nothing in their low bits (multiples of a power
  of two, aligned addresses) crowd into a few buckets whatever the size of the array: give such keys a hash that
  mixes. Keys whose low bits vary (ids, counters) are best left as they are: neighbouring keys keep neighbouring
  nodes, which a mixing hash would scatter (a find of a sequential key 25 ns against 60 mixed, measured).
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys: any object type that `Hash` hashes and `KeyEqual` compares. |
| `T` | The type of the mapped values: any object type the element is constructed with; `value_or` copies it. |
| `Hash` | A function object returning the `size_t` hash of a key. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. With `is_transparent` declared, as by `KeyEqual`, the lookups and `erase` take a key of another type. |
| `KeyEqual` | A function object comparing two keys for equality, consistent with `Hash`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |
| `value_type` | `pair<const Key, T>` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `iterator` | a forward iterator over `value_type` that holds its node, `std::forward_iterator` |
| `const_iterator` | the same over `const value_type` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](map/map.md) | constructs the map |
| `(destructor)` | frees the counters; leaves the nodes, the elements in them and the bucket array to the collector |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](map/begin.md) | an iterator to the first element of the list |
| [end, cend](map/end.md) | an iterator past the last element |

#### Capacity

| Function | Description |
|---|---|
| [empty](map/empty.md) | checks whether the map holds an element |
| [size](map/size.md) | the number of elements, the sum of the striped counts |

#### Modifiers

| Function | Description |
|---|---|
| [clear](map/clear.md) | erases every element there is |
| [insert](map/insert.md) | inserts an element, or the elements of a range, unless the key is taken |
| [emplace](map/emplace.md) | constructs an element in place unless the key is taken |
| [try_emplace](map/try_emplace.md) | inserts an element built from the key and arguments only when the key is absent |
| [erase](map/erase.md) | erases the element under a key, or at an iterator |

#### Lookup

| Function | Description |
|---|---|
| [count](map/count.md) | the number of elements under a key, 0 or 1 |
| [find](map/find.md) | an iterator to the element under a key |
| [contains](map/contains.md) | checks whether the map holds a key |
| [value_or](map/value_or.md) | a copy of the value under a key, or a fallback |

#### Bucket interface

| Function | Description |
|---|---|
| [bucket_count](map/bucket_count.md) | the number of buckets |

#### Hash policy

| Function | Description |
|---|---|
| [reserve](map/reserve.md) | grows the bucket array for a number of elements |

#### Observers

| Function | Description |
|---|---|
| [hash_function](map/hash_function.md) | a copy of the hash function |
| [key_eq](map/key_eq.md) | a copy of the equality of the keys |

## Complexity

- `find`, `contains`, `count`, `value_or`: constant on average, the walk from the bucket's dummy over the elements
  of the bucket, one on average at a load factor of one; linear in the size when every key falls into one bucket.
- `insert`, `emplace`, `try_emplace`, `erase`: the same search, then one compare-exchange; an insertion that
  doubles the array copies its slots, amortized constant.
- `size`, `bucket_count`: constant. `empty`, `begin`: constant, plus the dummies and erased nodes before the first
  element. `clear`: linear in the number of elements.

A node is allocated per element and one per bucket used. At sixteen threads a lookup is 4.9 ns and a mixed
operation 15, against 62 and 144 for `std::unordered_map` under a mutex
([Benchmarks: Concurrent containers](benchmarks.md#concurrent-containers)).

## Iterator invalidation

An iterator is never invalidated: it holds its node by a `tracked_ptr`, whatever the other threads insert or
erase and whether the array doubles. An iterator to an erased element still reads the element as it was, and its
increment goes on to the elements after it in the list. Iteration is weakly consistent, as Java's: it skips the
elements erased since it passed them and may or may not see the ones inserted meanwhile.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Count {
    atomic<int> n = 0;
};

int main() {
    concurrent::map<string, tracked_ptr<Count>> counts;
    vector<thread> threads;
    for (int t : range(8)) {
        threads.emplace_back([&counts, t] {
            for (int i : range(10000)) {
                string word = "w" + to_string((i * 8 + t) % 1000);
                // one Count per word, whichever thread gets there first
                auto [it, fresh] = counts.try_emplace(word, make_tracked<Count>());
                ++it->second->n;
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    int total = 0;
    for (auto& [word, count] : counts) {
        total += count->n;
    }
    println("{} words, {} occurrences, {} buckets", counts.size(), total, counts.bucket_count());
}
```

Output:

```text
1000 words, 80000 occurrences, 1024 buckets
```

## See also

- [set](set.md): the same table with the key as the element
- [sorted_map](sorted_map.md): the map in key order, a skip list, and the marker nodes both share
- [cache](cache.md), [weak_map](weak_map.md), [intern](intern.md): the structures over this table
- [map](../core/map.md): the sequential map with the full `std::unordered_map` interface
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](README.md#the-rules)
