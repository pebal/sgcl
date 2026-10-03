[sgcl](../../README.md) › [core](../README.md)

# sgcl::set\<Key, Hash, KeyEqual\>

```cpp
#include "sgcl/core/set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class set;
}
```

`sgcl::set<Key, Hash, KeyEqual>` is `std::unordered_set` over managed nodes: the same hash table as
[map](../map/README.md), holding keys alone. The interface is the one of `std::unordered_set` — the constructors,
insertion, erasure, node handles and `merge`, the lookups with a transparent hash and equality, forward
iterators, the bucket interface and the hash policy, `==`, the deduction guides and `std::erase_if` — and so is
the behaviour: unique keys, an element destroyed the moment it is erased, iterators valid across a rehash and
until their element is erased.

What differs from `std` is where the memory lives. The set holds two `tracked_ptr`s (the bucket array and a
sentinel node), the counts, the hasher and the equality, so it lives where a `tracked_ptr` may live; the elements
are nodes on the managed heap, forming one chain linked by tracked pointers and traced from the sentinel. A
`set<tracked_ptr<T>>` is therefore a set of traced pointers (`std::hash` of a `tracked_ptr` hashes the address),
and a cycle through a set is collected like any other. Nothing is freed by hand: an erasure destroys the element
and unlinks the node, and the collector reclaims the node later. The hash of each key is cached in its node. An
iterator is one raw node pointer, trivially copyable and storable anywhere. As in `std`, `iterator` and
`const_iterator` are one type, yielding `const Key&`: a key is never modified in place (the cached hash and the
bucket would no longer match); it is extracted, changed in its node handle and inserted back. Lookups and
iteration pay no write barrier; insertions, erasures and rehashes store tracked pointers and pay the barrier on
each link they relink ([README: Containers](../README.md#containers)).

Go has no set type; its idiom, a `map[K]struct{}`, is this container with the keys alone, except that Go
randomizes the order of every iteration, while here the order is the chain's and changes only with the set.

## Rules

- A set holds tracked pointers, so it lives on a stack or inside a managed object: never in `new`/`malloc`
  memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1). The same holds for a [node handle](../set-node_type/README.md).
- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the set is destroyed, exactly as in
  `std`. The one exception is a set dying in a sweep, inside a managed object nobody refers to any more: its
  nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector
  thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the set, across
  insertions, rehashes, erasures of other elements, `swap`, `merge` and a move of the set. An iterator to an
  erased element is invalid as in `std`.
- A key cannot be modified through an iterator (they yield `const Key&`); [extract](extract.md) it, change
  [value()](../set-node_type/value.md) of the handle and insert it back.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the
  element.
- Thread safety is that of `std::unordered_set`: concurrent readers, or one writer, with the program's own
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
| [node_type](../set-node_type/README.md) | the node handle, the same type as multiset's |
| `insert_return_type` | a struct `{ iterator position; bool inserted; node_type node; }`, the result of [insert](insert.md) of a node handle |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](set.md) | constructs the set |
| `(destructor)` | destroys the elements, or leaves them to the sweep the set dies in ([Rules](#rules)); the nodes, the bucket array and the sentinel are reclaimed by the collector |
| [operator=](operator_assign.md) | assigns the elements of another set or of a list |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the first element |
| [end, cend](end.md) | an iterator past the last element |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the set is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements a set may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element, keeps the buckets |
| [insert](insert.md) | inserts elements, or nodes, unless their keys are there |
| [emplace](emplace.md) | constructs an element in place unless its key is there |
| [emplace_hint](emplace_hint.md) | the same, with a hint that is ignored |
| [erase](erase.md) | erases elements |
| [swap](swap.md) | swaps the contents |
| [extract](extract.md) | takes a node out of the set, into a node handle |
| [merge](merge.md) | relinks the nodes of another set or multiset |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements with a key, 0 or 1 |
| [find](find.md) | an iterator to the element with a key |
| [contains](contains.md) | checks whether the set holds a key |
| [equal_range](equal_range.md) | the range of the elements with a key |

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
([mixin::enumerable](../mixin/enumerable/README.md)); `contains` is the set's own, by the key.

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
| [operator==, operator!=](operator_cmp.md) | compare the elements of two sets, in any order |
| [swap](swap.md) | swaps the contents of two sets |
| [erase_if](erase_if.md) | erases every element satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
         class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
set(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> set<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
set(std::initializer_list<Key>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> set<Key, Hash, KeyEqual>;
```

## Complexity

- `find`, `contains`, `count`, `equal_range`, and the insertion or the erasure of one element: constant on
  average, the walk of one bucket; linear in the size when every key falls into one bucket.
- A growth of the table relinks every node and hashes none: amortized constant per insertion.
- `clear`, a copy, `erase_if` and `==`: linear in the size.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `emplace`, `emplace_hint`, `rehash`, `reserve`, `max_load_factor` | never |
| `swap`, `merge`, the move constructor | never: an iterator follows its element into the other set |
| `erase`, `extract`, `erase_if` | only the iterators to the erased or extracted elements |
| `clear`, `operator=` | all |
| a rehash, by `rehash`, `reserve` or an insertion that grows the table | the local iterators |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    string name;
    set<tracked_ptr<Node>> peers;  // a set of traced pointers inside a managed object
};

int main() {
    println("a graph whose edges are sets");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    // A graph whose edges are sets: each node is reachable from its peers
    tracked_ptr a = make_tracked<Node>("a");
    tracked_ptr b = make_tracked<Node>("b");
    tracked_ptr c = make_tracked<Node>("c");
    a->peers.insert(b);
    b->peers.insert(a);  // a cycle
    b->peers.insert(c);
    bool again = b->peers.insert(c).second;  // false: a duplicate, nothing is inserted

    // A set of values on the stack, each name once
    set<string> names;
    for (const auto& peer : b->peers) {  // pointers hash by address: any order
        names.insert(peer->name);
    }
    names.insert("b");
    println("{} names, {} buckets", names.size(), names.bucket_count());

    // Dropping the stack roots: a and b keep each other alive only through
    // their sets, which the collector sees as a cycle
    a = b = c = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    // the nodes, buckets and sentinel of `names`
    println("{} live objects", collector::get_live_object_count() - base);
    return !again && names.size() == 3 ? 0 : 1;
}
```

Output:

```text
a graph whose edges are sets
3 names, 8 buckets
8 live objects
```

## See also

- [multiset](../multiset/README.md) for equal keys, [map](../map/README.md) and [multimap](../multimap/README.md) for key-value pairs,
  [ordered_set](../ordered_set/README.md) for the order of insertion, [sorted_set](../sorted_set/README.md) for an ordered tree
- [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
