[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>

```cpp
#include "sgcl/concurrent/set.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class set;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::set<Key, Hash, KeyEqual>` is a lock-free hash set shared by any number of threads: the
split-ordered list of Shalev and Shavit under [map](../map/README.md), which has the account of the algorithm, with the key
as the element. Every key sits in one sorted singly linked list ordered by the bit reversal of its hash, and an
array of buckets points into the list at dummy nodes; the array doubles as the keys outnumber the buckets and no
node moves, the old array left to the collector once nothing walks it
([README: Lock-free containers](../README.md#lock-free-containers)).

What differs from `std::unordered_set`: the interface has its names, restricted to what a lock-free set can offer
(`find`, `contains`, `count`, `insert`, `emplace`, `erase`, `clear`, `size`, `empty`, `bucket_count`, `reserve`,
iteration); there are no node handles and no iteration of one bucket. The elements are const, as in
`std::unordered_set`; an erased one is destroyed by the collector, not at the erase. What differs from Java's
`ConcurrentHashMap.newKeySet()`: no operation takes a lock, and `insert` is the set's own, searching once and
building the element only for a key that is absent. Go's library has no set.

## Rules

- The container holds its bucket array and its head node by `tracked_ptr`s, and its counters by a `std::unique_ptr`
  (plain memory of numbers alone, freed with the set). Its iterators hold their node by a `tracked_ptr`.
- Every member function may be called from any thread at any time, concurrently with any other. `find`,
  `contains` and `count` are wait-free once the key's bucket has its dummy node; `insert`, `emplace` and `erase`
  are lock-free and linearizable, an insertion at the compare-exchange that links its node into the list, an
  erasure at the one that marks it. Nothing waits but `reserve`, which may spin while another thread doubles the
  array.
- The count of the elements is striped over cache lines (Java's `LongAdder`): `size()` is the sum, a snapshot of no
  particular moment under concurrent modification, exact once the threads are quiet. The array doubles once the
  elements outnumber the buckets (a load factor of one), by the insertion that notices; `reserve` doubles it up
  front. The array never shrinks.
- An element is destroyed by the collector with its node, once nothing holds the node: not at the erase, which
  other threads may be reading it across. The element an iterator addresses stays alive as long as the iterator,
  erased or not.
- The dummies of the buckets used stay for the life of the container, one node each.
- The bucket of a key is the low bits of its hash, and `std::hash` of an integer is the integer itself (in libc++
  and libstdc++; of a pointer, the address, in libstdc++): keys with nothing in their low bits (multiples of a
  power of two, aligned addresses) crowd into a few buckets and want a hash that mixes; keys whose low bits vary
  are best left as they are ([map: Rules](../map/README.md#rules)).
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the elements: any object type that `Hash` hashes and `KeyEqual` compares. |
| `Hash` | A function object returning the `size_t` hash of a key. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. With `is_transparent` declared, as by `KeyEqual`, the lookups and `erase` take a key of another type. |
| `KeyEqual` | A function object comparing two keys for equality, consistent with `Hash`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `value_type` | `Key` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `iterator` | a forward iterator over `const Key` that holds its node, `std::forward_iterator` |
| `const_iterator` | the same type as `iterator` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](set.md) | constructs the set |
| `(destructor)` | frees the counters; leaves the nodes, the elements in them and the bucket array to the collector |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first element of the list |
| [end, cend](end.md) | an iterator past the last element |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the set holds an element |
| [size](size.md) | the number of elements, the sum of the striped counts |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every element there is |
| [insert](insert.md) | inserts a key, or the keys of a range, unless the key is there |
| [emplace](emplace.md) | constructs a key in place unless it is there |
| [erase](erase.md) | erases a key, or the element at an iterator |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements equal to a key, 0 or 1 |
| [find](find.md) | an iterator to the element equal to a key |
| [contains](contains.md) | checks whether the set holds a key |

#### Bucket interface

| Function | Description |
|---|---|
| [bucket_count](bucket_count.md) | the number of buckets |

#### Hash policy

| Function | Description |
|---|---|
| [reserve](reserve.md) | grows the bucket array for a number of elements |

#### Observers

| Function | Description |
|---|---|
| [hash_function](hash_function.md) | a copy of the hash function |
| [key_eq](key_eq.md) | a copy of the equality of the keys |

## Complexity

- `find`, `contains`, `count`: constant on average, the walk from the bucket's dummy over the elements of the
  bucket, one on average at a load factor of one; linear in the size when every key falls into one bucket.
- `insert`, `emplace`, `erase`: the same search, then one compare-exchange; an insertion that doubles the array
  copies its slots, amortized constant.
- `size`, `bucket_count`: constant. `empty`, `begin`: constant, plus the dummies and erased nodes before the first
  element. `clear`: linear in the number of elements.

A node is allocated per element and one per bucket used. The set is not measured on its own: it is the table
of the map, whose numbers are on [Benchmarks: Concurrent containers](../benchmarks.md#concurrent-containers).

## Iterator invalidation

An iterator is never invalidated: it holds its node by a `tracked_ptr`, whatever the other threads insert or
erase and whether the array doubles. An iterator to an erased element still reads it, and its increment goes on to
the elements after it in the list. Iteration is weakly consistent, as Java's: it skips the elements erased since it
passed them and may or may not see the ones inserted meanwhile.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<int> claimed;
    atomic<int> wins = 0;
    vector<thread> threads;
    for (int t : range(8)) {
        threads.emplace_back([&] {
            for (int id : range(10000)) {
                wins += claimed.insert(id).second;  // exactly one thread claims each id
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    println("{} claims, {} ids", wins.load(), claimed.size());
}
```

Output:

```text
10000 claims, 10000 ids
```

## See also

- [map](../map/README.md): the same table with a value under each key, and the account of the algorithm
- [sorted_set](../sorted_set/README.md): the set in key order, a skip list
- [weak_set](../weak_set/README.md), [intern](../intern/README.md): the sets over this table that hold their objects weakly
- [set](../../core/set/README.md): the sequential set
- [README: Lock-free containers](../README.md#lock-free-containers), [README: The rules](../README.md#the-rules)
