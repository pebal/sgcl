[sgcl](../README.md) › [core](README.md)

# sgcl::sorted_map\<Key, T, Compare\>

```cpp
#include "sgcl/core/sorted_map.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class Key, class T, class Compare = std::less<Key>>
    class sorted_map;
}
```

`sgcl::sorted_map<Key, T, Compare>` is `std::map` on a red-black tree whose nodes are managed objects. The
interface is the one of `std::map`: constructors, `insert`, `emplace`, `try_emplace`, `insert_or_assign`,
`operator[]`, `at`, `erase`, `extract`, `merge`, node handles, the lookups with transparent comparators,
bidirectional iterators (`std::ranges` algorithms work), `key_comp` and `value_comp`, `swap`, `==` and `<=>`,
`std::erase_if`. The behaviour is the one of `std::map` too: unique keys in `Compare` order, an element destroyed
the moment it is erased, iterators that stay valid until their element is erased. Beside them it has
[take](sorted_map/take.md), the value under a key moved out and the element erased, and the reads by the key of
[mixin::lookup](mixin/lookup.md).

What differs is where the memory lives. The map object holds one `tracked_ptr` (to a header node whose parent is
the root), a count and the comparator, so it lives where a `tracked_ptr` may live. Every node is a managed object
and the links between nodes are tracked pointers: the whole tree hangs off the header and is traced from there, so
a `sorted_map<Key, tracked_ptr<T>>` or a map inside a managed object is traced like any other managed data, and a
cycle through a map is collected like any other cycle. Nothing is freed by hand: an `erase` destroys the element
and unlinks the node, and the collector reclaims the node's memory later. Iterators are one raw node pointer
each, trivially copyable, and may live anywhere, a `std::vector` of them included: the map roots every node it
holds, and a raw pointer in a stack frame is a root of its own under the conservative scan. A lookup, an iteration
and an iterator copy read raw pointers only and pay no write barrier; an insertion, an erasure and a rebalancing
store tracked pointers and pay the barrier on each link they relink ([README: Containers](README.md#containers)).

The header node is allocated on the first insertion, so an empty map costs nothing and the default constructor
cannot throw.

## Rules

- A map holds a `tracked_ptr`, so it lives on a stack or inside a managed object: never in `new`/`malloc` memory,
  a `std` container, a global, a `thread_local` or a plain coroutine frame ([The rules](README.md#the-rules), 1).
  The same holds for a node handle ([node_type](sorted_map-node_type.md)), which holds the node through a
  `tracked_ptr`.
- The elements may hold tracked pointers (`sgcl::sorted_map<int, sgcl::tracked_ptr<T>>`, a key that is a
  `tracked_ptr`): the nodes are managed objects, so those pointers are traced.
- An element is destroyed the moment it is erased, cleared, assigned over, or the map is destroyed, exactly as in
  `std::map`. The one exception is a map dying in a sweep, inside a managed object nobody refers to any more: its
  nodes are garbage of the same sweep, and each destroys its element when the sweep reaches it, on a collector
  thread ([README: Threads](../async/README.md#threads)).
- An iterator, a reference or a pointer to an element is valid while the element is in the map, across
  insertions, erasures of other elements, `swap`, `merge` and a move of the map (it follows the node). An iterator
  to an erased element is invalid as in `std`; it keeps the node's memory mapped but not the element.
- A `tracked_ptr` may point at an element of a map, or a member of one: a node is a managed object, and an alias
  to a part of a managed object is allowed ([The rules](README.md#the-rules), 4). Such a pointer keeps the node
  alive, not the element, which dies with the erase.
- Thread safety is that of `std::map`: concurrent readers, or one writer, with the program's own synchronization
  ([The rules](README.md#the-rules), 6). The collector never waits for a mutator and never touches a node the map
  still links.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys, ordered by `Compare`. A key in the map is never changed; a key out of it, in a node handle, may be. |
| `T` | The type of the mapped values. |
| `Compare` | A strict weak ordering of the keys, `std::less<Key>` by default. When it declares `is_transparent` (as `std::less` of a [string](string.md) does), the lookups, `at`, `take`, `erase` and `extract` take a key of another type. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

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
| [node_type](sorted_map-node_type.md) | the node handle, which owns an element taken out of the map |
| `insert_return_type` | a struct `{ iterator position; bool inserted; node_type node; }`, the result of [insert(node_type&&)](sorted_map/insert.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sorted_map/sorted_map.md) | constructs the map |
| `(destructor)` | destroys the elements, from the largest key down; in a sweep leaves them to the same sweep; the nodes are the collector's |
| [operator=](sorted_map/operator_assign.md) | assigns values to the map |

#### Element access

| Function | Description |
|---|---|
| [at](sorted_map/at.md) | the value under a key, with bounds checking |
| [operator[]](sorted_map/operator_at.md) | the value under a key, inserted when absent |
| [min](sorted_map/min.md) | the element with the smallest key |
| [max](sorted_map/max.md) | the element with the largest key |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](sorted_map/begin.md) | an iterator to the beginning |
| [end, cend](sorted_map/end.md) | an iterator to the end |
| [rbegin, crbegin](sorted_map/rbegin.md) | a reverse iterator to the beginning |
| [rend, crend](sorted_map/rend.md) | a reverse iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](sorted_map/empty.md) | checks whether the map is empty |
| [size](sorted_map/size.md) | the number of elements |
| [max_size](sorted_map/max_size.md) | the largest number of elements a map may hold |

#### Modifiers

| Function | Description |
|---|---|
| [clear](sorted_map/clear.md) | destroys every element |
| [insert](sorted_map/insert.md) | inserts elements or nodes |
| [insert_or_assign](sorted_map/insert_or_assign.md) | inserts an element or assigns to the value under its key |
| [emplace](sorted_map/emplace.md) | constructs an element in place |
| [emplace_hint](sorted_map/emplace_hint.md) | constructs an element in place, with a hint |
| [try_emplace](sorted_map/try_emplace.md) | constructs an element in place only when the key is absent |
| [erase](sorted_map/erase.md) | erases elements |
| [take](sorted_map/take.md) | moves the value under a key out and erases the element |
| [swap](sorted_map/swap.md) | swaps the contents |
| [extract](sorted_map/extract.md) | takes a node out of the map, into a node handle |
| [merge](sorted_map/merge.md) | relinks the nodes of another map into this one |

#### Lookup

| Function | Description |
|---|---|
| [count](sorted_map/count.md) | the number of elements under a key, 0 or 1 |
| [find](sorted_map/find.md) | finds the element under a key |
| [contains](sorted_map/contains.md) | checks whether the map holds a key |
| [equal_range](sorted_map/equal_range.md) | the range of the elements under a key |
| [lower_bound](sorted_map/lower_bound.md) | the first element whose key is not less than a key |
| [upper_bound](sorted_map/upper_bound.md) | the first element whose key is greater than a key |

#### Observers

| Function | Description |
|---|---|
| [key_comp](sorted_map/key_comp.md) | the function that compares the keys |
| [value_comp](sorted_map/value_comp.md) | the function that compares the elements by their keys |

#### From mixin::enumerable

The questions asked of the elements, the pairs ([mixin::enumerable](mixin/enumerable.md)); `contains`, `min` and
`max` are the map's own, by the key and as the ends of the order.

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

The reads by the key that `std::map` makes a program write by hand ([mixin::lookup](mixin/lookup.md)).

| Function | Description |
|---|---|
| `get` | a copy of the value under a key, `nullopt` when absent |
| `try_get` | a pointer to the value under a key, null when absent |
| `value_or` | a copy of the value under a key, or a fallback |
| `contains_key` | checks whether the map holds a key |
| `keys` | the keys as a range, in order |
| `values` | the values as a range, in the order of their keys |
| `values_of` | the values under a key as a range |

## Non-member functions

| Function | Description |
|---|---|
| `operator==`, `operator<=>` | compare the elements in order: `==` one by one, `<=>` lexicographically, with the synthesized three-way comparison (the element's `<=>`, or a `std::weak_ordering` built from its `<`), so `!=`, `<`, `<=`, `>` and `>=` follow ([mixin::equatable](mixin/equatable.md), [mixin::comparable](mixin/comparable.md)) |
| [swap](sorted_map/swap.md) | swaps the contents of two maps |
| [erase_if](sorted_map/erase_if.md) | erases every element satisfying a predicate |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Compare = std::less<std::remove_const_t<
             typename std::iterator_traits<InputIt>::value_type::first_type>>>
sorted_map(InputIt, InputIt, Compare = Compare())
    -> sorted_map<std::remove_const_t<
                      typename std::iterator_traits<InputIt>::value_type::first_type>,
                  typename std::iterator_traits<InputIt>::value_type::second_type, Compare>;

template<class Key, class T, class Compare = std::less<Key>>
sorted_map(std::initializer_list<pair<Key, T>>, Compare = Compare())
    -> sorted_map<Key, T, Compare>;
```

From an iterator pair or an initializer list, as for `std::map`; an initializer list of a map spells its pairs
out (`std::pair{1, 2.0}`), since a braced pair alone names no type ([the constructors](sorted_map/sorted_map.md)).

## Complexity

- `find`, `count`, `contains`, `equal_range`, `lower_bound`, `upper_bound`, `at`: logarithmic in the size, reading
  raw pointers only.
- `insert`, `emplace`, `try_emplace`, `insert_or_assign`, `operator[]`, `erase` by key, `extract` by key:
  logarithmic; with a hint where the key belongs right before it, amortized constant, and an append in sorted
  order at `end()` costs one comparison. An insertion or an erasure pays the write barrier on each link it
  relinks.
- `begin`, `end`, `min`, `max`, `size`, `empty`: constant; the header keeps the leftmost and the rightmost node.
- The copy constructor and copy assignment: linear, the tree copied shape for shape with no comparison.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| all read-only operations, `insert`, `emplace`, `emplace_hint`, `try_emplace`, `insert_or_assign`, `operator[]`, `merge`, `swap` | never |
| `erase`, `take` | the erased elements only |
| `extract` | the extracted element only |
| `clear`, `operator=` | every element; `end()` stays, except after a move assignment |

An iterator is one raw node pointer and follows its node: after a `swap`, a `merge` or a move of the map it
points at the same element in the other map, `end()` included, which follows the header. Before the first
insertion there is no header: `begin()` and `end()` are both null iterators, and an `end()` taken then does not
compare equal to `end()` after the first insertion.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Account {
    string owner;
    int balance;
    tracked_ptr<Account> linked;  // a pointer inside an element: traced through the node
};

// The map object lives inside a managed object: the nodes hang off it and
// die with it, elements included, when the Bank is collected.
struct Bank {
    sorted_map<string, tracked_ptr<Account>> accounts;
};

int main() {
    println("a bank's accounts, by name");
    // after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    tracked_ptr bank = make_tracked<Bank>();
    for (const char* name : {"carol", "alice", "bob"}) {
        // operator[] inserts a null tracked_ptr; the object is made afterwards
        bank->accounts[name] = make_tracked<Account>(name, 100);
    }
    bank->accounts["alice"]->linked = bank->accounts["bob"];
    bank->accounts["bob"]->linked = bank->accounts["alice"];  // a cycle: collected like any other

    // A map on the stack, keys in order; the iterator follows the node
    sorted_map<string, int> balances;
    for (auto& [name, account] : bank->accounts) {  // alice bob carol
        balances.emplace(name, account->balance);
    }
    auto bob = balances.find("bob");
    balances.erase("alice");  // the int and the string die here
    balances.insert_or_assign("carol", 250);
    println("{} still there, {} balances", bob->first, balances.size());

    // Erasing from the bank drops the node; alice and bob keep each other
    // reachable only through their cycle, which the collector breaks
    bank->accounts.erase("alice");
    bank->accounts.erase("bob");
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    auto live = collector::get_live_object_count() - base;
    println("{} account left, {} live objects", bank->accounts.size(), live);
    return balances.size() == 2 && bank->accounts.size() == 1 ? 0 : 1;
}
```

Output:

```text
a bank's accounts, by name
bob still there, 2 balances
1 account left, 10 live objects
```

## See also

- [sorted_multimap](sorted_multimap.md) for equal keys, [sorted_set](sorted_set.md) and
  [sorted_multiset](sorted_multiset.md) for keys alone, [map](map.md) for a hash table
- [tracked_ptr](tracked_ptr.md), [unique_ptr](unique_ptr.md), [make_tracked](make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](README.md#the-rules),
  [README: Stack roots](../../garbage_collector/overview.md#stack-roots)
