[sgcl](../../README.md) › [core](../README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>

```cpp
#include "sgcl/core/ordered_set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class ordered_set;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::ordered_set<Key, Hash, KeyEqual>` is a hash set iterated in the order its elements were inserted: Java's
`LinkedHashSet`. It is [set](../set/README.md) with every node on a second list in the order of insertion, as
[ordered_map](../ordered_map/README.md) is to `map`: `begin()` to `end()` walks that list both ways (the iterators are
bidirectional, `rbegin` exists), `front()` is the oldest element and `back()` the newest, a copy keeps the order,
an erasure takes the element out of it, an insertion of an element that is there leaves it where it is, and
`to_back` and `to_front` move an element to the end or the start. The table, the lookups, the bucket interface,
the node handles, `merge` and the lookups by a key of another type are those of `set`; a rehash never touches the
order. Two words more per node.

What it is for: a set that is also a sequence without duplicates, kept in the order things arrived: the distinct
values of a stream in first-seen order, a list of names with no repeats, a set of visited nodes reported in the
order of the visit; and a set with an eviction order, as the map has.

Where the memory lives is `set`'s layout. The set object holds two `tracked_ptr`s, the bucket array and the
sentinel node, which is both the head of the chain and the end of the order, and beside them the counts, the hash
and the equality. Every element is a node on the managed heap, on one chain linked by tracked pointers and on
the list of the order, all traced from the sentinel: an `ordered_set<tracked_ptr<T>>` is a set of traced
pointers, and a cycle through a set is collected like any other. The hash of each element is cached in its node.
As in `std`, `iterator` and `const_iterator` are one type, yielding `const Key&`: an element is never modified in
place (its cached hash and its bucket would no longer match); `extract` it, change it in the handle and insert it
back. A lookup and an iteration pay no write barrier; an insertion, an erasure, a rehash, `to_back` and
`to_front` store tracked pointers and pay the barrier on each link they change
([README: Containers](../README.md#containers)).

## Rules

- The elements may be, or hold, tracked pointers (a `tracked_ptr` is hashed by its address): the nodes are
  managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the set is destroyed, exactly as in
  `std`. The one exception is a set dying in a sweep, inside a managed object nobody refers to any more: its
  nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector
  thread ([README: Threads](../../async/README.md#threads)).
- An iterator, a reference or a pointer to an element is valid while the element is in the set, across
  insertions, rehashes, erasures of other elements, `to_back` and `to_front` of any element, `swap`, `merge` and a
  move of the set: it follows the node. An iterator to an erased element is invalid, as in `std`. Iterators are
  trivially copyable, one raw node pointer each, and may live anywhere: the set roots every node it links.
- `end()` is the sentinel, so `--end()` is the newest element. A set that has never had a bucket array (made
  empty, nothing inserted yet) has no sentinel: its `end()` is null, is not decremented, and is not the `end()`
  of the set after its first insertion ([Iterator invalidation](#iterator-invalidation)).
- An element cannot be modified through an iterator; [extract](extract.md) it, change `value()` of
  the handle and insert it back.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::unordered_set`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the elements: any object type that `Hash` hashes and `KeyEqual` compares. |
| `Hash` | A function object returning the `size_t` hash of an element. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. With `is_transparent` declared, as by `KeyEqual`, the lookups, `erase`, `extract` and `bucket` take a key of another type. |
| `KeyEqual` | A function object comparing two elements for equality, consistent with `Hash`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `value_type` | `Key` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `reference` | `value_type&` |
| `const_reference` | `const value_type&` |
| `pointer` | `value_type*` |
| `const_pointer` | `const value_type*` |
| `const_iterator` | a bidirectional iterator over `const Key` in the order of insertion, one raw node pointer, `std::bidirectional_iterator` |
| `iterator` | `const_iterator` |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |
| `const_local_iterator` | a forward iterator over the elements of one bucket, in the order of the chain, `std::forward_iterator` |
| `local_iterator` | `const_local_iterator` |
| [node_type](../ordered_set-node_type/README.md) | the node handle |
| `insert_return_type` | a struct `{ iterator position; bool inserted; node_type node; }`, the result of `insert(node_type&&)` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ordered_set.md) | constructs the set |
| `(destructor)` | destroys the elements, in a sweep leaves them to the sweep; the nodes, the bucket array and the sentinel are left to the collector |
| [operator=](operator_assign.md) | assigns values to the set |

#### Element access

| Function | Description |
|---|---|
| [front](front.md) | the oldest element |
| [back](back.md) | the newest element |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the oldest element |
| [end, cend](end.md) | the iterator past the newest element |
| [rbegin, crbegin](rbegin.md) | a reverse iterator to the newest element |
| [rend, crend](rend.md) | the reverse iterator past the oldest element |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the set is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | erases every element |
| [insert](insert.md) | inserts elements or nodes |
| [emplace](emplace.md) | constructs an element in place |
| [emplace_hint](emplace_hint.md) | constructs an element in place, with a hint |
| [erase](erase.md) | erases elements |
| [swap](swap.md) | swaps the contents |
| [extract](extract.md) | takes a node out of the set |
| [merge](merge.md) | relinks the nodes of another set |
| [to_back](to_back.md) | moves an element to the end of the order |
| [to_front](to_front.md) | moves an element to the start of the order |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements equal to a key |
| [find](find.md) | an iterator to the element equal to a key |
| [contains](contains.md) | checks whether an element is there |
| [equal_range](equal_range.md) | the range of the elements equal to a key |

#### Bucket interface

| Function | Description |
|---|---|
| [begin(size_type), cbegin(size_type)](begin.md) | a local iterator to the first element of a bucket |
| [end(size_type), cend(size_type)](end.md) | the local iterator past the last element of a bucket |
| [bucket_count](bucket_count.md) | the number of buckets |
| [max_bucket_count](max_bucket_count.md) | the largest number of buckets |
| [bucket_size](bucket_size.md) | the number of elements in a bucket |
| [bucket](bucket.md) | the bucket of an element |

#### Hash policy

| Function | Description |
|---|---|
| [load_factor](load_factor.md) | the average number of elements per bucket |
| [max_load_factor](max_load_factor.md) | the load factor at which the table grows, read or set |
| [rehash](rehash.md) | sets the number of buckets |
| [reserve](reserve.md) | makes room for a number of elements |

#### Observers

| Function | Description |
|---|---|
| [hash_function](hash_function.md) | a copy of the hash function |
| [key_eq](key_eq.md) | a copy of the equality of the elements |

#### From mixin::enumerable

The questions asked of the elements, in the order of insertion ([mixin::enumerable](../mixin/enumerable/README.md));
`contains` is the set's own, by the hash.

| Function | Description |
|---|---|
| `index_of` | the position in the order of the element equal to a value |
| `last_index_of` | the position in the order of the last element equal to a value |
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
| [operator==](operator_cmp.md) | compares the contents of two sets, whatever their orders |
| [swap](swap.md) | swaps the contents of two sets |
| [erase_if](erase_if.md) | erases the elements a predicate accepts |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
         class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
ordered_set(InputIt, InputIt, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> ordered_set<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
ordered_set(std::initializer_list<Key>, size_t = 0, Hash = Hash(), KeyEqual = KeyEqual())
    -> ordered_set<Key, Hash, KeyEqual>;
```

## Complexity

- `find`, `contains`, `count`, `equal_range`, `insert`, `emplace`, `erase` by a key, `extract`: constant on
  average, the walk of one bucket; linear in the size when every element falls into one bucket. An insertion is
  amortized over the growths of the table.
- `begin`, `end`, `front`, `back`, `to_back`, `to_front`, `size`, `empty`, a step of an iterator, `swap`:
  constant.
- `clear`, `rehash`, `reserve`, `erase_if`, a copy, `==`: linear in the size.

A node is allocated per element: the element, the link of the chain, the cached hash and the two links of the
order, two words more than a node of [set](../set/README.md).

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `to_back`, `to_front`, `max_load_factor` | never |
| `rehash`, `reserve` | no iterator but an `end()` taken while the set had no bucket array, and the local ones (the last row) |
| `swap` | never: the iterators, `end()` included, follow their elements and the sentinel into the other set |
| `insert`, `emplace`, `emplace_hint`, `merge` | no iterator but an `end()` taken while the set had no bucket array, and the local ones when the table grows (the last row) |
| `erase`, `extract`, `erase_if` | the erased elements only |
| `clear` | every element; not `end()` |
| `operator=` | always |
| a rehash, by `rehash`, `reserve` or an insertion that grows the table | the local iterators |

A move constructor takes the elements and the sentinel to the new set: the iterators into the old one, `end()`
included, are iterators into the new one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    string name;
    vector<tracked_ptr<Node>> edges;
};

// The nodes reachable from `start`, each once, in the order the walk
// first reaches them: the set is the visited set and the report at once
ordered_set<tracked_ptr<Node>> reach(const tracked_ptr<Node>& start) {
    ordered_set<tracked_ptr<Node>> visited;
    vector<tracked_ptr<Node>> stack = {start};
    while (!stack.empty()) {
        tracked_ptr node = stack.back();
        stack.pop_back();
        if (visited.insert(node).second) {  // new: its edges next
            for (const auto& edge : node->edges) {
                stack.push_back(edge);
            }
        }
    }
    return visited;
}

int main() {
    tracked_ptr a = make_tracked<Node>("a");
    tracked_ptr b = make_tracked<Node>("b");
    tracked_ptr c = make_tracked<Node>("c");
    a->edges = {b, c};
    b->edges = {a};  // a cycle
    c->edges = {b};
    for (const auto& node : reach(a)) {
        print("{} ", node->name);
    }
    println();
    return 0;
}
```

Output:

```text
a c b 
```

## See also

- [ordered_map](../ordered_map/README.md): key-value pairs in the order of insertion
- [set](../set/README.md): the same set without the order
- [sorted_set](../sorted_set/README.md): the set in the order of the keys
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
