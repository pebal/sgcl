[sgcl](../../README.md) › [core](../README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>

```cpp
#include "sgcl/core/multiset.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class multiset;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::multiset<Key, Hash, KeyEqual>` is `std::unordered_multiset` over managed nodes: the same hash table as
[set](../set/README.md), with equivalent keys allowed. The interface is the one of `std::unordered_multiset` — the
constructors, insertion, erasure, node handles and `merge`, the lookups with a transparent hash and equality,
forward iterators, the bucket interface and the hash policy, `==`, the deduction guides and `std::erase_if` — and
so is the behaviour: equal elements are adjacent in the order of iteration and in their bucket, `erase` of a key
removes all of them, `count` counts them, and an element is destroyed the moment it is erased. Within a run of
equal elements a new one goes in front of those already there.

What differs from `std` is where the memory lives. The container holds two `tracked_ptr`s (the bucket array and a
sentinel node), the counts, the hasher and the equality, so it lives where a `tracked_ptr` may live; the elements
are nodes on the managed heap, forming one chain linked by tracked pointers and traced from the sentinel, so
elements that are or hold `tracked_ptr`s are traced and a cycle through the container is collected like any
other. Nothing is freed by hand: an erasure destroys the element and unlinks the node, and the collector reclaims
the node later. The hash of each key is cached in its node. An iterator is one raw node pointer, trivially
copyable and storable anywhere, valid across rehashes and until its element is erased. As in `std`, `iterator`
and `const_iterator` are one type, yielding `const Key&`: a key is never modified in place; it is extracted,
changed in its node handle and inserted back. Lookups and iteration pay no write barrier; insertions, erasures
and rehashes store tracked pointers and pay the barrier on each link they relink
([README: Containers](../README.md#containers)).

Go has no multiset; its idiom, a `map[K]int` of counts, keeps one key and a number, where this container keeps
every element inserted, equal or not.

## Rules

- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the container is destroyed, exactly
  as in `std`. The one exception is a container dying in a sweep, inside a managed object nobody refers to any
  more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a
  collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the container, across
  insertions, rehashes, erasures of other elements, `swap`, `merge` and a move of the container. An iterator to
  an erased element is invalid as in `std`.
- A key cannot be modified through an iterator (they yield `const Key&`); [extract](extract.md) it,
  change [value()](../set-node_type/value.md) of the handle and insert it back.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the
  element.
- Thread safety is that of `std::unordered_multiset`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the elements, the keys: any object type that `Hash` hashes and `KeyEqual` compares. An operation that copies or moves elements requires `Key` to be copyable or movable. |
| `Hash` | A function object returning the `size_t` hash of a key. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. With `is_transparent` declared, as by `KeyEqual`, the lookups, `erase`, `extract` and `bucket` take a key of another type. |
| `KeyEqual` | A function object comparing two keys for equality, consistent with `Hash`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `value_type` | `Key` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `value_type&` |
| `const_reference` | `const value_type&` |
| `pointer` | `value_type*` |
| `const_pointer` | `const value_type*` |
| `iterator` | the same type as `const_iterator` |
| `const_iterator` | a forward iterator over `const Key`, one raw node pointer, `std::forward_iterator` |
| `local_iterator` | the same type as `const_local_iterator` |
| `const_local_iterator` | a forward iterator over `const Key` that stops at the end of its bucket, `std::forward_iterator` |
| [node_type](../set-node_type/README.md) | the node handle, the same type as set's |

There is no `insert_return_type`: every [insert](insert.md) of a multiset returns an iterator.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](multiset.md) | constructs the multiset |
| `(destructor)` | destroys the elements, or leaves them to the sweep the container dies in ([Rules](#rules)); the nodes, the bucket array and the sentinel are reclaimed by the collector |
| [operator=](operator_assign.md) | assigns the elements of another multiset or of a list |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first element |
| [end, cend](end.md) | an iterator past the last element |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the multiset is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements a multiset may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element, keeps the buckets |
| [insert](insert.md) | inserts elements, or nodes |
| [emplace](emplace.md) | constructs an element in place |
| [emplace_hint](emplace_hint.md) | the same, with a hint that is ignored |
| [erase](erase.md) | erases elements |
| [swap](swap.md) | swaps the contents |
| [extract](extract.md) | takes a node out of the multiset, into a node handle |
| [merge](merge.md) | relinks every node of another multiset or set |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements with a key |
| [find](find.md) | an iterator to the first element with a key |
| [contains](contains.md) | checks whether the multiset holds a key |
| [equal_range](equal_range.md) | the run of the elements with a key |

#### Bucket interface

| Function | Description |
|---|---|
| [begin(size_type), cbegin(size_type)](begin.md) | an iterator to the first element of a bucket |
| [end(size_type), cend(size_type)](end.md) | an iterator past the last element of a bucket |
| [bucket_count](bucket_count.md) | the number of buckets |
| [max_bucket_count](max_bucket_count.md) | the largest number of buckets |
| [bucket_size](bucket_size.md) | the number of elements in a bucket |
| [bucket](bucket.md) | the bucket of a key |

#### Hash policy

| Function | Description |
|---|---|
| [load_factor](load_factor.md) | the average number of elements per bucket |
| [max_load_factor](max_load_factor.md) | the load factor at which the table grows, read or set |
| [rehash](rehash.md) | sets the number of buckets |
| [reserve](reserve.md) | sets the number of buckets for a number of elements |

#### Observers

| Function | Description |
|---|---|
| [hash_function](hash_function.md) | a copy of the hash function |
| [key_eq](key_eq.md) | a copy of the equality of the keys |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)); `contains` is the multiset's own, by the key.

| Function | Description |
|---|---|
| `index_of` | the position of the first element equal to a value, in the order of iteration |
| `last_index_of` | the position of the last element equal to a value |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the elements of two multisets, in any order |
| [swap](swap.md) | swaps the contents of two multisets |
| [erase_if](erase_if.md) | erases every element satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
         class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
multiset(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> multiset<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
multiset(std::initializer_list<Key>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> multiset<Key, Hash, KeyEqual>;
```

## Complexity

- `find`, `contains` and the insertion of one element: constant on average, the walk of one bucket; linear in
  the size when every key falls into one bucket. `count`, `equal_range` and `erase` of a key: the same, plus the
  length of the key's run.
- A growth of the table relinks every node and hashes none: amortized constant per insertion.
- `clear`, a copy, `erase_if` and `==`: linear in the size, `==` quadratic in the length of the longest run.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `emplace`, `emplace_hint`, `rehash`, `reserve`, `max_load_factor` | never |
| `swap`, `merge`, the move constructor | never: an iterator follows its element into the other container |
| `erase`, `extract`, `erase_if` | only the iterators to the erased or extracted elements |
| `clear`, `operator=` | all |
| a rehash, by `rehash`, `reserve` or an insertion that grows the table | the local iterators |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Sample {
    string source;
    tracked_ptr<Sample> previous;  // traced through the node that holds the Sample
};

int main() {
    println("readings, each value as often as it came");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    // A bag of readings keyed by value: several samples may read the same
    multiset<int> readings;
    for (int r : {3, 7, 3, 3, 9, 7}) {
        readings.insert(r);
    }
    println("3 read {} times, 7 read {} times", readings.count(3), readings.count(7));

    // A bag of traced pointers inside a managed object: the samples live as
    // long as the bag's owner does
    struct Owner {
        multiset<tracked_ptr<Sample>> bag;
    };
    tracked_ptr owner = make_tracked<Owner>();
    tracked_ptr first = make_tracked<Sample>("a");
    owner->bag.insert(first);
    owner->bag.insert(first);  // the same pointer twice: a multiset allows it
    owner->bag.insert(make_tracked<Sample>("b", first));
    auto duplicates = owner->bag.count(first);  // 2

    // Erasing every copy of the pointer destroys those elements; the Sample
    // itself stays while `first` or "b" refers to it
    owner->bag.erase(first);
    first = nullptr;
    owner = nullptr;  // the bag, "b" and then "a" are garbage
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    // the nodes, buckets and sentinel of `readings`
    println("{} live objects", collector::get_live_object_count() - base);
    return readings.count(3) == 3 && duplicates == 2 ? 0 : 1;
}
```

Output:

```text
readings, each value as often as it came
3 read 3 times, 7 read 2 times
8 live objects
```

## See also

- [set](../set/README.md) for unique keys, [multimap](../multimap/README.md) for key-value pairs,
  [sorted_multiset](../sorted_multiset/README.md) for an ordered tree
- [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
