[sgcl](../../README.md) › [core](../README.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>

```cpp
#include "sgcl/core/multimap.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class multimap;
}
```

`sgcl::multimap<Key, T, Hash, KeyEqual>` is `std::unordered_multimap` over managed nodes: the same hash table as
[map](../map/README.md), with equivalent keys allowed. The interface is the one of `std::unordered_multimap` — the
constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with a transparent hash
and equality, forward iterators, the bucket interface, the hash policy, `swap`, `==`, the deduction guides,
`std::erase_if` — with the reads of [mixin::lookup](../mixin/lookup/README.md) beside it, and so is the behaviour: elements
with equivalent keys are adjacent in the iteration order and in their bucket, `erase(key)` removes all of them,
`count` counts them, an element is destroyed the moment it is erased. Within a run of equal keys a new element
goes in front of those already there.

What differs from `std` is where the memory lives. The container holds two `tracked_ptr`s (the bucket array and a
sentinel node), the counts, the hasher and the equality, so it lives where a `tracked_ptr` may live; the elements
are nodes on the managed heap, forming one chain linked by tracked pointers and traced from the sentinel, so
elements holding `tracked_ptr`s are traced and a cycle through the container is collected like any other. Nothing
is freed by hand: an `erase` destroys the element and unlinks the node, the collector reclaims the node later. The
hash of each key is cached in its node. The bucket count is 0 or a power of two, and the table grows when the size
reaches `bucket_count() * max_load_factor()`, doubling at least, to eight buckets at the least. Iterators are one
raw node pointer each, trivially copyable, storable anywhere, valid across rehashes and until their element is
erased.

What differs from Go: its library has no multimap; a Go program keeps a slice per key in a `map`, which a
multimap does with one node per element and no slice to grow.

## Rules

- A multimap holds tracked pointers, so it lives on a stack or inside a managed object: never in
  `new`/`malloc` memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1). The same holds for a node handle ([node_type](../map-node_type/README.md)).
- The elements may hold tracked pointers (a `tracked_ptr` key hashes by address through
  `std::hash<sgcl::tracked_ptr<T>>`): the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the container is destroyed, exactly
  as in `std`. The one exception is a container dying in a sweep, inside a managed object nobody refers to any
  more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a
  collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the container
  ([Iterator invalidation](#iterator-invalidation)); iterators may live anywhere, a `std::vector<iterator>`
  included.
- A `tracked_ptr` may point at an element or a member of one (a node is a managed object); it keeps the node
  alive, not the element.
- Thread safety is that of `std::unordered_multimap`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys, hashed by `Hash` and compared by `KeyEqual`. A `tracked_ptr` key is hashed by its address. |
| `T` | The type of the mapped values. |
| `Hash` | A function object returning the `size_t` hash of a key. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. With `is_transparent` declared, as by `KeyEqual`, the lookups take a key of another type. |
| `KeyEqual` | A function object comparing two keys for equality, consistent with `Hash`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |
| `value_type` | `std::pair<const Key, T>` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `value_type&` |
| `const_reference` | `const value_type&` |
| `pointer` | `value_type*` |
| `const_pointer` | `const value_type*` |
| `iterator` | one raw node pointer in a class of the library, `std::forward_iterator`; converts to `const_iterator` |
| `const_iterator` | the same over `const value_type` |
| `local_iterator` | an iterator over the nodes of one bucket, which stops at the end of the bucket: the node pointer, the bucket's index and the mask, `std::forward_iterator`; converts to `const_local_iterator` |
| `const_local_iterator` | the same over `const value_type` |
| [node_type](../map-node_type/README.md) | the node handle, the same type as map's |

There is no `insert_return_type`: every [insert](insert.md) of a multimap returns an iterator.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](multimap.md) | constructs the multimap |
| `(destructor)` | destroys the elements, on a stack or inside a managed object destroyed by hand; in a sweep leaves them to the sweep; the nodes, the bucket array and the sentinel are left to the collector |
| [operator=](operator_assign.md) | assigns values to the multimap |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the beginning |
| [end, cend](end.md) | an iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the multimap is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements a multimap may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element, keeps the buckets |
| [insert](insert.md) | inserts elements or nodes |
| [emplace](emplace.md) | constructs an element in place |
| [emplace_hint](emplace_hint.md) | constructs an element in place, with a hint |
| [erase](erase.md) | erases elements |
| [swap](swap.md) | swaps the contents |
| [extract](extract.md) | unlinks an element into a node handle |
| [merge](merge.md) | relinks the nodes of another multimap or map |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements under a key |
| [find](find.md) | an iterator to the first element under a key |
| [contains](contains.md) | checks whether the multimap holds a key |
| [equal_range](equal_range.md) | the range of the elements under a key |

#### Bucket interface

| Function | Description |
|---|---|
| [begin(size_type), cbegin(size_type)](begin.md) | an iterator to the beginning of a bucket |
| [end(size_type), cend(size_type)](end.md) | an iterator to the end of a bucket |
| [bucket_count](bucket_count.md) | the number of buckets |
| [max_bucket_count](max_bucket_count.md) | the largest number of buckets |
| [bucket_size](bucket_size.md) | the number of elements in a bucket |
| [bucket](bucket.md) | the bucket of a key |

#### Hash policy

| Function | Description |
|---|---|
| [load_factor](load_factor.md) | the average number of elements per bucket |
| [max_load_factor](max_load_factor.md) | the load factor the table grows at, read or set |
| [rehash](rehash.md) | sets the number of buckets |
| [reserve](reserve.md) | sets the number of buckets for a number of elements |

#### Observers

| Function | Description |
|---|---|
| [hash_function](hash_function.md) | a copy of the hash function |
| [key_eq](key_eq.md) | a copy of the equality of the keys |

#### From mixin::enumerable

The questions asked of the elements, the pairs, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)); `contains` is the multimap's own, by the key.

| Function | Description |
|---|---|
| `index_of` | the position of the first element equal to a pair |
| `last_index_of` | the position of the last element equal to a pair |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::lookup

The reads by the key ([mixin::lookup](../mixin/lookup/README.md)).

| Function | Description |
|---|---|
| `get` | a copy of the first value under a key, `nullopt` when absent |
| `try_get` | a pointer to the first value under a key, null when absent |
| `value_or` | a copy of the first value under a key, or a fallback |
| `contains_key` | checks whether the multimap holds a key |
| `keys` | the keys as a range, one per element |
| `values` | the values as a range |
| `values_of` | every value under a key, as a range |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the elements, each run of a key in any order |
| [swap](swap.md) | swaps the contents of two multimaps |
| [erase_if](erase_if.md) | erases every element satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<
             std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
         class KeyEqual = std::equal_to<
             std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
multimap(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> multimap<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
                typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
multimap(std::initializer_list<pair<Key, T>>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> multimap<Key, T, Hash, KeyEqual>;
```

The key and the mapped type are deduced from an iterator pair over pairs, or from an initializer list of
`pair`s, which must be spelled out ([(constructor)](multimap.md)).

## Complexity

- Lookup and insertion of one element: constant on average, linear in the size when every key falls into one
  bucket. The cached hash is compared before the key.
- `count`, `equal_range` and `erase` of a key: constant on average, plus the length of the key's run.
- An insertion that grows the table relinks every node, hashing nothing: amortized constant.
- Iteration: a load per step. Lookups and iteration pay no write barrier; insertions, erasures and rehashes store
  tracked pointers and pay the barrier on each link they relink ([README: Containers](../README.md#containers)).

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `emplace`, `emplace_hint`, `rehash`, `reserve`, `max_load_factor`, `swap`, `merge` | never |
| `erase`, `erase_if`, `extract` | only those to the erased (extracted) elements |
| `clear`, `operator=` | all of this multimap's |
| a rehash, by `rehash`, `reserve` or an insertion that grows the table | the local iterators |

An iterator follows its node: across a rehash, a `swap`, a `merge` and a move of the container it points at the
same element, now in whichever container holds the node. A rehash keeps the runs of equal keys together and in
their order. An iterator to an erased element is invalid as in `std`. `end()` is a null iterator and never
changes; a local iterator holds the bucket and the mask, and a rehash, the growth of an insertion included,
invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    string name;
    tracked_ptr<Listener> forward_to;  // traced through the node that holds the Listener
};

int main() {
    println("listeners by topic");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    // Several listeners per topic: a multimap of traced pointers on the stack
    multimap<string, tracked_ptr<Listener>> topics;
    tracked_ptr logger = make_tracked<Listener>("logger");
    topics.emplace("error", logger);
    topics.emplace("error", make_tracked<Listener>("pager", logger));
    topics.emplace("info", logger);
    topics.emplace("info", make_tracked<Listener>("stats"));

    // The run of one key: every listener of "error"
    auto [from, to] = topics.equal_range("error");
    print("error ->");
    for (auto it = from; it != to; ++it) {
        print(" {}", it->second->name);  // the newest first
    }
    println();

    // Erasing a whole key destroys its tracked_ptr elements at once; the
    // pager is collected, the logger lives on under "info"
    auto erased = topics.erase("error");
    logger = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    println("{} erased, {} under info, {} live objects", erased, topics.count("info"),
            collector::get_live_object_count() - base);
    return erased == 2 && topics.size() == 2 ? 0 : 1;
}
```

Output:

```text
listeners by topic
error -> pager logger
2 erased, 2 under info, 10 live objects
```

## See also

- [map](../map/README.md) for unique keys, [multiset](../multiset/README.md) for keys alone, [sorted_multimap](../sorted_multimap/README.md) for
  an ordered tree
- [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
