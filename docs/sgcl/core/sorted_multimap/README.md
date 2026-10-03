[sgcl](../../README.md) › [core](../README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>

```cpp
#include "sgcl/core/sorted_multimap.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Compare = std::less<Key>>
    class sorted_multimap;
}
```

`sgcl::sorted_multimap<Key, T, Compare>` is `std::multimap` on a red-black tree whose nodes are managed objects:
the same tree as [sorted_map](../sorted_map/README.md), with equivalent keys allowed. The interface is the one of
`std::multimap` (constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with
transparent comparators, bidirectional iterators, `key_comp` and `value_comp`, `swap`, `==` and `<=>`,
`std::erase_if`), and so is the behaviour: elements with equivalent keys are adjacent, a new one goes after the
ones already there (insertion order within a key is kept), `erase(key)` removes all of them, an element is
destroyed the moment it is erased. Beside them it has the reads by the key of [mixin::lookup](../mixin/lookup/README.md):
`get` the first value under a key, `values_of` all of them.

What differs from `std` is where the memory lives. The multimap object holds one `tracked_ptr` (to a header node),
a count and the comparator, so it lives where a `tracked_ptr` may live; the nodes are managed objects linked by
tracked pointers, traced from the header, so elements holding `tracked_ptr`s are traced and a cycle through a
multimap is collected like any other. Nothing is freed by hand: an `erase` destroys the element and unlinks the
node, the collector reclaims the node later. Iterators are one raw node pointer each, trivially copyable,
storable anywhere, valid while their element is in the container. Lookups and iteration read raw pointers and pay
no write barrier; insertions, erasures and rebalancing store tracked pointers and pay the barrier on each link
they relink ([README: Containers](../README.md#containers)). The header is allocated on the first insertion: an empty
multimap costs nothing.

## Rules

- A multimap holds a `tracked_ptr`, so it lives on a stack or inside a managed object: never in `new`/`malloc`
  memory, a `std` container, a global, a `thread_local` or a plain coroutine frame
  ([The rules](../README.md#the-rules), 1). The same holds for a node handle
  ([node_type](../sorted_map-node_type/README.md)).
- The elements may hold tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the multimap is destroyed, exactly
  as in `std`. The one exception is a multimap dying in a sweep, inside a managed object nobody refers to any
  more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a
  collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the container, across
  insertions, erasures of other elements, `swap`, `merge` and a move of the container. An iterator to an erased
  element is invalid as in `std`.
- A `tracked_ptr` may point at an element or a member of one (a node is a managed object); it keeps the node
  alive, not the element.
- Thread safety is that of `std::multimap`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys, ordered by `Compare`; equivalent keys are allowed. A key in the multimap is never changed; a key out of it, in a node handle, may be. |
| `T` | The type of the mapped values. |
| `Compare` | A strict weak ordering of the keys, `std::less<Key>` by default. When it declares `is_transparent` (as `std::less` of a [string](../string/README.md) does), the lookups, `erase` and `extract` take a key of another type. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |
| `value_type` | `std::pair<const Key, T>` |
| `key_compare` | `Compare` |
| `value_compare` | a function object comparing two elements by their keys with `Compare` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `value_type&` |
| `const_reference` | `const value_type&` |
| `pointer` | `value_type*` |
| `const_pointer` | `const value_type*` |
| `iterator` | a `std::bidirectional_iterator` over `value_type`: one raw node pointer, trivially copyable |
| `const_iterator` | the same over `const value_type`; an `iterator` converts to it, not back |
| `reverse_iterator` | `std::reverse_iterator<iterator>` |
| `const_reverse_iterator` | `std::reverse_iterator<const_iterator>` |
| [node_type](../sorted_map-node_type/README.md) | the node handle, the same type as sorted_map's |

There is no `insert_return_type`: every `insert` of a multimap returns an iterator.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_multimap.md) | constructs the multimap |
| `(destructor)` | destroys the elements, from the largest key down; in a sweep leaves them to the same sweep; the nodes are the collector's |
| [operator=](operator_assign.md) | assigns values to the multimap |

#### Element access

| Function | Description |
|---|---|
| [min](min.md) | the first element with the smallest key |
| [max](max.md) | the last element with the largest key |

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
| [empty](empty.md) | checks whether the multimap is empty |
| [size](size.md) | the number of elements |
| [max_size](max_size.md) | the largest number of elements a multimap may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](clear.md) | destroys every element |
| [insert](insert.md) | inserts elements or nodes |
| [emplace](emplace.md) | constructs an element in place |
| [emplace_hint](emplace_hint.md) | constructs an element in place, with a hint |
| [erase](erase.md) | erases elements |
| [swap](swap.md) | swaps the contents |
| [extract](extract.md) | takes a node out of the multimap, into a node handle |
| [merge](merge.md) | relinks the nodes of another map into this one |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements under a key |
| [find](find.md) | finds the first element under a key |
| [contains](contains.md) | checks whether the multimap holds a key |
| [equal_range](equal_range.md) | the range of the elements under a key |
| [lower_bound](lower_bound.md) | the first element whose key is not less than a key |
| [upper_bound](upper_bound.md) | the first element whose key is greater than a key |

#### Observers

| Function | Description |
|---|---|
| [key_comp](key_comp.md) | the function that compares the keys |
| [value_comp](value_comp.md) | the function that compares the elements by their keys |

#### From mixin::enumerable

The questions asked of the elements, the pairs ([mixin::enumerable](../mixin/enumerable/README.md)); `contains`, `min` and
`max` are the multimap's own, by the key and as the ends of the order.

| Function | Description |
|---|---|
| `index_of` | the position of the first element equal to a pair |
| `last_index_of` | the position of the last element equal to a pair |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `for_each` | calls a function with every element |

#### From mixin::lookup

The reads by the key that `std::multimap` makes a program write by hand ([mixin::lookup](../mixin/lookup/README.md)); a read
of one value reads the first under the key.

| Function | Description |
|---|---|
| `get` | a copy of the first value under a key, `nullopt` when absent |
| `try_get` | a pointer to the first value under a key, null when absent |
| `value_or` | a copy of the first value under a key, or a fallback |
| `contains_key` | checks whether the multimap holds a key |
| `keys` | the keys as a range, in order, one per element |
| `values` | the values as a range, in the order of their keys |
| `values_of` | every value under a key as a range, in insertion order |

## Non-member functions

| Function | Description |
|---|---|
| `operator==`, `operator<=>` | compare the elements in iteration order: `==` one by one, `<=>` lexicographically, with the synthesized three-way comparison, so `!=`, `<`, `<=`, `>` and `>=` follow ([mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md)) |
| [swap](swap.md) | swaps the contents of two multimaps |
| [erase_if](erase_if.md) | erases every element satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Compare = std::less<std::remove_const_t<
             typename std::iterator_traits<InputIt>::value_type::first_type>>>
sorted_multimap(InputIt, InputIt, Compare = Compare())
    -> sorted_multimap<std::remove_const_t<
                           typename std::iterator_traits<InputIt>::value_type::first_type>,
                       typename std::iterator_traits<InputIt>::value_type::second_type, Compare>;

template<class Key, class T, class Compare = std::less<Key>>
sorted_multimap(std::initializer_list<pair<Key, T>>, Compare = Compare())
    -> sorted_multimap<Key, T, Compare>;
```

From an iterator pair or an initializer list, as for `std::multimap`; an initializer list of a map spells its
pairs out (`std::pair{1, 2.0}`), since a braced pair alone names no type
([the constructors](sorted_multimap.md)).

## Complexity

- `find`, `contains`, `equal_range`, `lower_bound`, `upper_bound`: logarithmic in the size, reading raw pointers
  only; `count` logarithmic plus the number of elements under the key.
- `insert`, `emplace`: logarithmic; with a hint where the element belongs right before it, amortized constant, and
  an append in sorted order at `end()` costs one comparison. `erase` by key: logarithmic plus the number erased. An
  insertion or an erasure pays the write barrier on each link it relinks.
- `begin`, `end`, `min`, `max`, `size`, `empty`: constant; the header keeps the leftmost and the rightmost node.
- The copy constructor and copy assignment: linear, the tree copied shape for shape with no comparison.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `emplace`, `emplace_hint`, `merge`, `swap` | never |
| `erase` | the erased elements only |
| `extract` | the extracted element only |
| `clear`, `operator=` | every element; `end()` stays, except after a move assignment |

An iterator is one raw node pointer and follows its node: after a `swap`, a `merge` or a move of the multimap it
points at the same element in the other container, `end()` included, which follows the header. Before the first
insertion there is no header: `begin()` and `end()` are both null iterators, and an `end()` taken then does not
compare equal to `end()` after the first insertion.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Entry {
    string what;
    tracked_ptr<Entry> cause;  // traced through the node that holds the Entry
};

int main() {
    println("a log, by day");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    // A multimap on the stack: several events per day, in the order they came
    sorted_multimap<int, tracked_ptr<Entry>> log;
    tracked_ptr first = make_tracked<Entry>("boot");
    log.emplace(1, first);
    log.emplace(1, make_tracked<Entry>("login", first));
    log.emplace(2, make_tracked<Entry>("logout"));
    log.emplace(2, make_tracked<Entry>("shutdown"));
    log.emplace(1, make_tracked<Entry>("late entry"));  // goes after the other day-1 events

    // The run of one key, in insertion order
    auto [from, to] = log.equal_range(1);
    print("day 1:");
    for (auto it = from; it != to; ++it) {
        print(" {}", it->second->what);
    }
    println();

    // Erasing a whole key destroys its elements (the tracked_ptrs) at once;
    // the Events they pointed at are collected, except "boot", still held by `first`
    auto erased = log.erase(1);
    first = nullptr;  // now "boot" is unreachable too
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    auto live = collector::get_live_object_count() - base;
    println("{} erased, {} left, {} live objects", erased, log.size(), live);
    return erased == 3 && log.count(2) == 2 ? 0 : 1;
}
```

Output:

```text
a log, by day
day 1: boot login late entry
3 erased, 2 left, 7 live objects
```

## See also

- [sorted_map](../sorted_map/README.md) for unique keys, [sorted_multiset](../sorted_multiset/README.md) for keys alone,
  [multimap](../multimap/README.md) for a hash table
- [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
