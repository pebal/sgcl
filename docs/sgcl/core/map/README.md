[sgcl](../../README.md) › [core](../README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>

```cpp
#include "sgcl/core/map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map;
}
```

`sgcl::map<Key, T, Hash, KeyEqual>` is `std::unordered_map` over managed nodes. The interface is the one of
`std::unordered_map` — the constructors, `insert`, `emplace`, `try_emplace`, `insert_or_assign`, `operator[]`,
`at`, `erase`, `extract`, `merge`, node handles, the lookups with a transparent hash and equality, forward
iterators (`std::ranges` algorithms work), the bucket interface with local iterators, the hash policy, `swap`,
`==`, the deduction guides, `std::erase_if` — with [take](take.md) and the reads of
[mixin::lookup](../mixin/lookup/README.md) beside it. The behaviour is the one of `std::unordered_map` too: unique keys, an
element destroyed the moment it is erased, iterators that stay valid across a rehash and until their element is
erased.

What differs is where the memory lives and a few details of the layout. The map object holds two `tracked_ptr`s
(the bucket array, a managed array of node pointers, and a sentinel node that precedes the first element), the
counts, the hasher and the equality, so it lives where a `tracked_ptr` may live. Every element is a node on the
managed heap and the nodes form one chain linked by tracked pointers, as in libstdc++: a bucket points at the node
before its first node, so the whole chain is traced from the sentinel, an iterator is one raw node pointer, and an
iteration is a load per step. A `map<Key, tracked_ptr<T>>`, or one inside a managed object, is traced like any
other managed data, and a cycle through it is collected like any other cycle. Nothing is freed by hand: an `erase`
destroys the element and unlinks the node, the collector reclaims the node later. The hash of each key is cached
in its node, so a rehash hashes nothing and a lookup compares hashes before keys. The bucket count is 0 or a power
of two: the table starts with no bucket array at all and grows when the size reaches
`bucket_count() * max_load_factor()`, doubling at least, to eight buckets at the least
([rehash](rehash.md), [bucket](bucket.md)).

What differs from Go's `map`: the order of iteration is the chain's, not shuffled from one loop to the next, and an
element has an address: a reference or an iterator to it stays valid until it is erased.

## Rules

- A map holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc`
  memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1). The same holds for a node handle ([node_type](../map-node_type/README.md)), which
  holds its node through a `tracked_ptr`.
- The elements may hold tracked pointers (`sgcl::map<int, sgcl::tracked_ptr<T>>`, or a `tracked_ptr` key:
  `std::hash<sgcl::tracked_ptr<T>>` hashes the address): the nodes are managed objects, so those pointers are
  traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the map is destroyed, exactly as in
  `std`. The one exception is a map dying in a sweep, inside a managed object nobody refers to any more: its nodes
  are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector thread
  ([README: Threads](../../async/README.md#threads)).
- An iterator, a reference or a pointer to an element is valid while the element is in the map
  ([Iterator invalidation](#iterator-invalidation)). Iterators are trivially copyable and may live anywhere, a
  `std::vector<iterator>` included: the map roots every node it links.
- A `tracked_ptr` may point at an element of the map, or a member of one (a node is a managed object,
  [The rules](../README.md#the-rules), 4); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_map`: concurrent readers, or one writer, with the program's own
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
| [node_type](../map-node_type/README.md) | the node handle, the same type as multimap's |
| `insert_return_type` | `struct { iterator position; bool inserted; node_type node; }`, the result of [insert](insert.md) of a node handle |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](map.md) | constructs the map |
| `(destructor)` | destroys the elements, on a stack or inside a managed object destroyed by hand; in a sweep leaves them to the sweep; the nodes, the bucket array and the sentinel are left to the collector |
| [operator=](operator_assign.md) | assigns values to the map |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the beginning |
| [end, cend](end.md) | an iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the map is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements a map may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element, keeps the buckets |
| [insert](insert.md) | inserts elements or nodes |
| [insert_or_assign](insert_or_assign.md) | inserts an element, or assigns to the value under its key |
| [emplace](emplace.md) | constructs an element in place |
| [emplace_hint](emplace_hint.md) | constructs an element in place, with a hint |
| [try_emplace](try_emplace.md) | constructs an element in place only when the key is absent |
| [erase](erase.md) | erases elements |
| [take](take.md) | moves the value under a key out and erases the element |
| [swap](swap.md) | swaps the contents |
| [extract](extract.md) | unlinks an element into a node handle |
| [merge](merge.md) | relinks the nodes of another map |

#### Lookup

| Function | Description |
|---|---|
| [at](at.md) | access the value under a key, with bounds checking |
| [operator[]](operator_at.md) | access the value under a key, inserting one when absent |
| [count](count.md) | the number of elements under a key, 0 or 1 |
| [find](find.md) | an iterator to the element under a key |
| [contains](contains.md) | checks whether the map holds a key |
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
([mixin::enumerable](../mixin/enumerable/README.md)); `contains` is the map's own, by the key.

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
| `get` | a copy of the value under a key, `nullopt` when absent |
| `try_get` | a pointer to the value under a key, null when absent |
| `value_or` | a copy of the value under a key, or a fallback |
| `contains_key` | checks whether the map holds a key |
| `keys` | the keys as a range |
| `values` | the values as a range |
| `values_of` | the values under a key as a range |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | compare the elements, in any order |
| [swap](swap.md) | swaps the contents of two maps |
| [erase_if](erase_if.md) | erases every element satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<
             std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
         class KeyEqual = std::equal_to<
             std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
map(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
           typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
map(std::initializer_list<pair<Key, T>>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> map<Key, T, Hash, KeyEqual>;
```

The key and the mapped type are deduced from an iterator pair over pairs, or from an initializer list of
`pair`s, which must be spelled out: nested braces deduce nothing, as with `std` ([(constructor)](map.md)).

## Complexity

- Lookup, insertion and erasure of one element: constant on average, linear in the size when every key falls
  into one bucket. The cached hash is compared before the key.
- An insertion that grows the table relinks every node, hashing nothing: amortized constant.
- Iteration: a load per step. A lookup, an iteration and an iterator copy read raw pointers only and pay no write
  barrier; an insertion, an erasure and a rehash store tracked pointers and pay the barrier on each link they
  relink ([README: Containers](../README.md#containers)).

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `insert_or_assign`, `emplace`, `emplace_hint`, `try_emplace`, `operator[]`, `rehash`, `reserve`, `max_load_factor`, `swap`, `merge` | never |
| `erase`, `erase_if`, `take`, `extract` | only those to the erased (taken, extracted) elements |
| `clear`, `operator=` | all of this map's |
| a rehash, by `rehash`, `reserve` or an insertion that grows the table | the local iterators |

An iterator follows its node: across a rehash, a `swap`, a `merge` and a move of the map it points at the same
element, now in whichever map holds the node. An iterator to an erased element is invalid as in `std`; it keeps
the node's memory mapped but not the element. `end()` is a null iterator and never changes; a local iterator
holds the bucket and the mask, and a rehash, the growth of an insertion included, invalidates it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
    tracked_ptr<Node> next;  // a pointer inside an element: traced through the node
};

// The map object lives inside a managed object: its nodes hang off it and
// die with it, elements included, when the Registry is collected.
struct Registry {
    map<int, tracked_ptr<Node>> by_id;
};

int main() {
    println("a registry of nodes, by id");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    tracked_ptr registry = make_tracked<Registry>();
    for (int id : range(100)) {
        // operator[] inserts a null tracked_ptr; the object is made afterwards
        registry->by_id[id] = make_tracked<Node>(id);
    }
    registry->by_id[0]->next = registry->by_id[1];
    registry->by_id[1]->next = registry->by_id[0];  // a cycle: collected like any other

    // A map on the stack, keyed by pointer: std::hash<tracked_ptr> hashes the address
    map<tracked_ptr<Node>, string> names;
    names.try_emplace(registry->by_id[0], "zero");
    names.emplace(registry->by_id[1], "one");
    auto one = names.find(registry->by_id[1]);
    names.rehash(256);  // `one` is still valid
    println("{} is node {}", one->second, one->first->id);

    // Erasing from the registry destroys the tracked_ptr elements at once;
    // nodes 0 and 1 stay reachable through `names`, the rest is garbage
    std::erase_if(registry->by_id, [](const auto& p) { return p.first >= 2; });
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    println("{} in the registry, {} live objects", registry->by_id.size(),
            collector::get_live_object_count() - base);
    return registry->by_id.size() == 2 && names.size() == 2 ? 0 : 1;
}
```

Output:

```text
a registry of nodes, by id
one is node 1
2 in the registry, 13 live objects
```

## See also

- [multimap](../multimap/README.md) for equal keys, [set](../set/README.md) and [multiset](../multiset/README.md) for keys alone,
  [ordered_map](../ordered_map/README.md) for the order of insertion, [sorted_map](../sorted_map/README.md) for an ordered tree
- [concurrent::map](../../concurrent/map/README.md): the hash map shared by threads
- [tracked_ptr](../tracked_ptr/README.md), [unique_ptr](../unique_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules),
  [Stack roots](../../../garbage_collector/overview.md#stack-roots)
