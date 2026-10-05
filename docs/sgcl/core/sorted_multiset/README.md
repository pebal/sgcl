[sgcl](../../README.md) › [core](../README.md)

# sgcl::sorted_multiset\<Key, Compare\>

```cpp
#include "sgcl/core/sorted_multiset.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class Compare = std::less<Key>>
    class sorted_multiset;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::sorted_multiset<Key, Compare>` is `std::multiset` on a red-black tree whose nodes are managed objects: the
same tree as [sorted_set](../sorted_set/README.md), with equivalent keys allowed. The interface is the one of
`std::multiset` (the constructors, `insert`, `emplace`, `erase`, `extract`, `merge`, node handles, the lookups with
a transparent comparison, bidirectional iterators, `key_comp` and `value_comp`, `swap`, `==` and `<=>`,
`std::erase_if`), and so is the behaviour: equivalent keys are adjacent and a new one goes after those already
there, `erase(key)` removes all of them, `iterator` is `const_iterator`, an element is destroyed the moment it is
erased.

What differs from `std` is where the memory lives. The multiset object holds one `tracked_ptr` (to the header node
of the tree), a count and the comparison, so it lives where a `tracked_ptr` may. The nodes are managed objects
linked by tracked pointers and traced from the header, so elements that are or hold `tracked_ptr`s are traced, and a
cycle through a multiset is collected like any other. Nothing is freed by hand: an erase destroys the element and
unlinks its node, and the collector reclaims the node later. An iterator is one raw node pointer, trivially copyable
and storable anywhere, valid while its element is in the multiset. A lookup and an iteration read raw pointers and
pay no write barrier; an insertion, an erasure and the rebalancing store tracked pointers and pay the barrier on
each link they change ([README: Containers](../README.md#containers)). The header is made on the first insertion: an
empty multiset allocates nothing.

## Rules

- The elements may be, or hold, tracked pointers: the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared or assigned over, or when the multiset is destroyed,
  exactly as in `std`. The one exception is a multiset dying in a sweep, inside a managed object nobody refers to
  any more: its nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a
  collector thread.
- An iterator, a reference or a pointer to an element is valid while the element is in the multiset, across
  insertions, erasures of other elements, `swap`, `merge` and a move of the multiset. An iterator to an erased
  element is invalid, as in `std`.
- A `tracked_ptr` may point at an element (a node is a managed object); it keeps the node alive, not the element.
- Thread safety is that of `std::multiset`: concurrent readers, or one writer, with the program's own
  synchronization ([The rules](../README.md#the-rules), 6).

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
| [node_type](../sorted_set-node_type/README.md) | the node handle, the same type as sorted_set's |

There is no `insert_return_type`: every `insert` returns an iterator.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_multiset.md) | constructs the multiset |
| `(destructor)` | destroys the elements, unless the multiset dies in a sweep, which destroys them itself; the nodes are left to the collector |
| [operator=](operator_assign.md) | assigns another multiset or a list |

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
| [empty](empty.md) | checks whether the multiset is empty |
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
| [extract](extract.md) | takes a node out of the multiset |
| [merge](merge.md) | relinks the nodes of another multiset or set |

#### Lookup

| Function | Description |
|---|---|
| [count](count.md) | the number of elements with a key |
| [find](find.md) | the first element with a key |
| [contains](contains.md) | checks whether the multiset holds a key |
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
([mixin::enumerable](../mixin/enumerable/README.md)); `contains` is the multiset's own, by the key, and so are `min` and
`max`, the ends of the order. The multiset carries [mixin::equatable](../mixin/equatable/README.md),
[mixin::comparable](../mixin/comparable/README.md) and the bidirectional category too ([the mixins](../mixin/README.md)); not
[mixin::ordered](../mixin/ordered/README.md), the order being the multiset's own, nor [mixin::sequence](../mixin/sequence/README.md).

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
| `operator==`, `operator<=>` | compare the elements in order, as for `std::multiset`: `==` one by one, `<=>` lexicographically (through `<` for elements without `<=>`), and `!=`, `<`, `<=`, `>`, `>=` follow ([mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md)) |
| [swap](swap.md) | swaps the contents of two multisets |
| [erase_if](erase_if.md) | erases the elements a predicate accepts |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Compare = std::less<typename std::iterator_traits<InputIt>::value_type>>
sorted_multiset(InputIt, InputIt, Compare = Compare())
    -> sorted_multiset<typename std::iterator_traits<InputIt>::value_type, Compare>;

template<class Key, class Compare = std::less<Key>>
sorted_multiset(std::initializer_list<Key>, Compare = Compare())
    -> sorted_multiset<Key, Compare>;
```

## Complexity

- A lookup (`find`, `contains`, the bounds), an insertion and an erasure by iterator or by one key: logarithmic in
  the size; `count` and `erase(key)` add the number of elements with the key.
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

An iterator survives a `swap` and a move of the multiset: it points into the other multiset then, `end()` included,
which follows the header. Before the first insertion there is no header: `begin()` and `end()` are both null
iterators, and an `end()` taken then does not compare equal to `end()` after the first insertion.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Task {
    string name;
    int priority;
    tracked_ptr<Task> blocked_by;
};

// Tasks ordered by priority, several per level: a multiset of pointers
// with a comparator that looks through them
struct ByPriority {
    bool operator()(const tracked_ptr<Task>& l, const tracked_ptr<Task>& r) const noexcept {
        return l->priority < r->priority;
    }
};

int main() {
    println("tasks, by priority");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    sorted_multiset<tracked_ptr<Task>, ByPriority> queue;
    tracked_ptr build = make_tracked<Task>("build", 1);
    queue.insert(build);
    queue.insert(make_tracked<Task>("test", 2, build));
    queue.insert(make_tracked<Task>("lint", 2));  // after "test": equal keys keep their order
    queue.insert(make_tracked<Task>("deploy", 3));

    print("order:");
    for (const auto& task : queue) {
        print(" {}", task->name);
    }
    println();

    // Everything at priority 2 goes: the two tracked_ptrs are destroyed now,
    // "test" and "lint" are collected, "build" stays through `build`
    tracked_ptr probe = make_tracked<Task>("", 2);  // a key to look up with
    auto erased = queue.erase(probe);
    build = nullptr;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    auto live = collector::get_live_object_count() - base;
    println("{} erased, {} left, {} live objects", erased, queue.size(), live);
    return erased == 2 && queue.size() == 2 ? 0 : 1;
}
```

Output:

```text
tasks, by priority
order: build test lint deploy
2 erased, 2 left, 8 live objects
```

## See also

- [sorted_set](../sorted_set/README.md) for unique keys, [sorted_multimap](../sorted_multimap/README.md) for key-value pairs,
  [multiset](../multiset/README.md) for a hash table
- [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
