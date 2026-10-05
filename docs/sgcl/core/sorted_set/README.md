[sgcl](../../README.md) › [core](../README.md)

# sgcl::sorted_set\<Key, Compare\>

```cpp
#include "sgcl/core/sorted_set.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Compare = std::less<Key>>
    class sorted_set;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::sorted_set<Key, Compare>` is `std::set` on a red-black tree whose nodes are managed objects: the same tree
as [sorted_map](../sorted_map/README.md), holding keys alone. The interface is the one of `std::set` (the constructors,
`insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with a transparent comparison,
bidirectional iterators, `key_comp` and `value_comp`, `swap`, `==` and `<=>`, `std::erase_if`), and so is the
behaviour: unique keys in the order of `Compare`, elements that are not changed through an iterator (`iterator` is
`const_iterator`), an element destroyed the moment it is erased.

What differs from `std` is where the memory lives. The set object holds one `tracked_ptr` (to the header node of
the tree), a count and the comparison, so it lives where a `tracked_ptr` may. The nodes are managed objects linked
by tracked pointers and traced from the header, so a `sorted_set<tracked_ptr<T>>` is a set of traced pointers (a
`tracked_ptr` compares with `<=>`, so it is a key as it is), and a cycle through a set is collected like any other.
Nothing is freed by hand: an erase destroys the element and unlinks its node, and the collector reclaims the node
later. An iterator is one raw node pointer, trivially copyable and storable anywhere, valid while its element is in
the set. A lookup and an iteration read raw pointers and pay no write barrier; an insertion, an erasure and the
rebalancing store tracked pointers and pay the barrier on each link they change
([README: Containers](../README.md#containers)). The header is made on the first insertion: an empty set allocates
nothing.

## Rules

- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared or assigned over, or when the set is destroyed, exactly
  as in `std`. The one exception is a set dying in a sweep, inside a managed object nobody refers to any more: its
  nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector
  thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the set, across insertions,
  erasures of other elements, `swap`, `merge` and a move of the set. An iterator to an erased element is invalid,
  as in `std`.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::set`: concurrent readers, or one writer, with the program's own synchronization
  ([The rules](../README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the elements, the keys: any object type `Compare` orders. |
| `Compare` | A function object ordering two keys, a strict weak order. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. With `is_transparent` declared, as by `std::less` of a [string](../string/README.md), the lookups, `erase` and `extract` take a key of another type. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `value_type` | `Key` |
| `key_compare` | `Compare` |
| `value_compare` | `Compare` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `value_type&` |
| `const_reference` | `const value_type&` |
| `pointer` | `value_type*` |
| `const_pointer` | `const value_type*` |
| `iterator` | a bidirectional iterator over `const Key`, one raw node pointer, `std::bidirectional_iterator` |
| `const_iterator` | `iterator`, the same type |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |
| [node_type](../sorted_set-node_type/README.md) | the node handle, the same type as sorted_multiset's |
| `insert_return_type` | a struct `{ iterator position; bool inserted; node_type node; }`, the result of `insert(node_type&&)` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_set.md) | constructs the set |
| `(destructor)` | destroys the elements, unless the set dies in a sweep, which destroys them itself; the nodes are left to the collector |
| [operator=](operator_assign.md) | assigns another set or a list |

#### Element access

| Function | Description |
|---|---|
| [min](min.md) | the smallest element |
| [max](max.md) | the largest element |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](begin.md) | an iterator to the beginning |
| [end, cend](end.md) | an iterator to the end |
| [rbegin, crbegin](rbegin.md) | a reverse iterator to the beginning |
| [rend, crend](rend.md) | a reverse iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the set is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element |
| [insert](insert.md) | inserts elements or nodes |
| [emplace](emplace.md) | constructs an element in place |
| [emplace_hint](emplace_hint.md) | constructs an element in place, with a hint |
| [erase](erase.md) | erases elements |
| [swap](swap.md) | swaps the contents |
| [extract](extract.md) | takes a node out of the set |
| [merge](merge.md) | relinks the nodes of another set |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements with a key, 1 or 0 |
| [find](find.md) | the element with a key |
| [contains](contains.md) | checks whether the set holds a key |
| [equal_range](equal_range.md) | the range of the elements with a key |
| [lower_bound](lower_bound.md) | the first element not less than a key |
| [upper_bound](upper_bound.md) | the first element greater than a key |

#### Observers

| Function | Description |
|---|---|
| [key_comp](key_comp.md) | the comparison of the keys |
| [value_comp](value_comp.md) | the comparison of the elements, the same |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../mixin/enumerable/README.md)); `contains` is the set's own, by the key, and so are `min` and `max`,
the ends of the order. The set carries [mixin::equatable](../mixin/equatable/README.md),
[mixin::comparable](../mixin/comparable/README.md) and the bidirectional category too ([the mixins](../mixin/README.md)); not
[mixin::ordered](../mixin/ordered/README.md), the order being the set's own, nor [mixin::sequence](../mixin/sequence/README.md).

| Function | Description |
|---|---|
| `index_of` | the position of the first element equal to a value, in the order |
| `last_index_of` | the position of the last element equal to a value |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `for_each` | calls a function with every element |

## Non-member functions

| Function | Description |
|---|---|
| `operator==`, `operator<=>` | compare the elements in order, as for `std::set`: `==` one by one, `<=>` lexicographically (through `<` for elements without `<=>`), and `!=`, `<`, `<=`, `>`, `>=` follow ([mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md)) |
| [swap](swap.md) | swaps the contents of two sets |
| [erase_if](erase_if.md) | erases the elements a predicate accepts |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Compare = std::less<typename std::iterator_traits<InputIt>::value_type>>
sorted_set(InputIt, InputIt, Compare = Compare())
    -> sorted_set<typename std::iterator_traits<InputIt>::value_type, Compare>;

template<class Key, class Compare = std::less<Key>>
sorted_set(std::initializer_list<Key>, Compare = Compare())
    -> sorted_set<Key, Compare>;
```

## Complexity

- A lookup (`find`, `contains`, `count`, the bounds), an insertion and an erasure by key: logarithmic in the size.
- `begin`, `min`, `max`, `size`: constant. An erasure at an iterator and an insertion at the right hint:
  amortized constant.
- A copy: linear, the tree copied shape for shape with no comparison.

A node is allocated per element; the header on the first insertion.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `emplace`, `emplace_hint`, `swap`, `merge` | never |
| `erase`, `extract` | the erased or extracted elements only |
| `clear`, `operator=` | every element; `end()` stays, except after a move assignment |

An iterator survives a `swap` and a move of the set: it points into the other set then, `end()` included, which
follows the header. Before the first insertion there is no header: `begin()` and `end()` are both null
iterators, and an `end()` taken then does not compare equal to `end()` after the first insertion.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    string name;
    sorted_set<tracked_ptr<Node>> peers;  // a set of traced pointers inside a managed object
};

int main() {
    println("a graph whose edges are sorted sets");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    // A graph whose edges are sets: each node is reachable from its peers
    tracked_ptr a = make_tracked<Node>("a");
    tracked_ptr b = make_tracked<Node>("b");
    tracked_ptr c = make_tracked<Node>("c");
    a->peers.insert(b);
    b->peers.insert(a);  // a cycle
    b->peers.insert(c);
    b->peers.insert(c);  // a duplicate: nothing is inserted

    // A set of values on the stack: keys in order, each exactly once
    sorted_set<string> names;
    for (const auto& peer : b->peers) {  // pointers compare by address: any order
        names.insert(peer->name);
    }
    names.insert("b");
    print("b's neighbourhood:");
    for (const auto& n : names) {
        print(" {}", n);
    }
    println();

    // Dropping the stack roots: a and b keep each other alive only through
    // their sets, which the collector sees as a cycle
    std::size_t node_count = names.size();
    a = b = c = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    // what is left: the header and the three nodes of `names`, and their three strings
    println("{} live objects", collector::get_live_object_count() - base);
    return node_count == 3 ? 0 : 1;
}
```

Output:

```text
a graph whose edges are sorted sets
b's neighbourhood: a b c
7 live objects
```

## See also

- [sorted_multiset](../sorted_multiset/README.md) for equal keys, [sorted_map](../sorted_map/README.md) and
  [sorted_multimap](../sorted_multimap/README.md) for key-value pairs, [set](../set/README.md) for a hash table
- [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
