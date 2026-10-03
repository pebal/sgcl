[sgcl](../README.md) › [immutable](README.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>

```cpp
#include "sgcl/immutable/map.h"   // or "sgcl/immutable.h"

namespace sgcl::immutable {
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class map;
}
```

`sgcl::immutable::map<Key, T, Hash, KeyEqual>` is the immutable hash map, the persistent map of Clojure and Scala:
a map every `insert`, `set` and `erase` of which returns a new map and leaves the old one exactly as it was, the
two sharing everything but the path that changed. It is a hash array mapped trie, Bagwell's (*Ideal Hash Trees*,
2001) as Clojure's `PersistentHashMap` has it: a node has 32 slots, one per value of the five bits of the hash its
level consumes, and stores only the slots in use, packed in slot order behind a bitmap, so that the entry of a
slot is at the population count of the bits below it. An entry holds an element or the subtrie of the elements
whose hashes agree with it so far, and two elements with the same hash to the last bit hang off one entry as a
chain. A lookup walks log32(*n*) nodes, four for a million elements, and compares the key once; an `insert` or an
`erase` copies those nodes, a few hundred bytes, and shares the rest, and a subtrie left with one element is
folded into its parent, so that a map erased down to nothing holds no node at all.

The map is two words and its function objects: the size and a `tracked_ptr` to the root. A copy of it is a copy
of those words. The nodes are managed objects that no version owns, made in six sizes (room for 1, 2, 4, 8, 16 or
32 entries) so that each is one object with a destructor for its elements: a node reached by ten versions is one
node, and the collector destroys its elements and frees it once the last version that reaches it is dropped, the
accounting a persistent map without a collector does with a reference count per node.

What differs from `std::unordered_map` and Go's `map`: nothing is ever modified, so a map held by any number of
threads is read by all of them without a lock, and a version is published, and replaced by the next, through a
[concurrent::copy_on_write](../concurrent/copy_on_write.md) or an [atomic](../core/atomic.md). `insert` keeps an
element that is there, as every `insert` of the library does; `set` puts the value in its place. Many changes at
once go through a [builder](map-builder.md), the transient of Clojure and immer.

## Rules

- A map holds its root by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed
  object ([The rules](../core/README.md#the-rules), 1).
- Every member but the assignment is `const`. `insert`, `set`, `emplace` and `erase` return the new map; the one
  they were called on is unchanged, and stays so for as long as it is held. The elements are `const` through the
  map.
- A change copies the elements of the nodes on the path, up to 32 per node: the copy constructors of `Key` and `T`
  are what a change costs, plus the nodes.
- A key or a value holding tracked pointers is traced where it lives, in a node; one with a destructor is
  destroyed when the collector frees its node, once no version reaches it. Nothing is destroyed by an `erase`: the
  old version still holds the element.
- Its iterators, and the pointers `try_get` hands back, hold nothing alive: valid while the map object they came
  from exists and holds the same version, as `std`'s are.
- With a transparent hash and equality (`is_transparent`, as `std::hash` and `std::equal_to` of a
  [string](../core/string.md) are) the lookups take a key of another type and build none: a `string_view` or a
  literal finds a `string` key with no string made for the search.
- Sharing between threads: any number of threads read any version. A map variable that one thread replaces while
  others read it is the one thing that needs synchronization, and
  [concurrent::copy_on_write](../concurrent/copy_on_write.md) is the shape for it: `load()` is one atomic load for
  a snapshot, `update(f)` copies two words, applies `f` (an `insert`, an `erase`) and swings the pointer, so an
  update costs O(log *n*) where a `copy_on_write` over the mutable [map](../core/map.md) costs a copy of
  everything. An [atomic](../core/atomic.md) `tracked_ptr` to a map does the same with the version in a managed
  object of its own.
- The order of iteration is the trie's, the bits of the hashes; it changes with nothing but the elements.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the keys. A change copies the keys of the nodes on its path, so it requires `Key` to be copy constructible. |
| `T` | The type of the values, copy constructible for the same reason. |
| `Hash` | The hash of a key, `size_t operator()(const Key&)`. With a member type `is_transparent`, together with `KeyEqual`'s, the lookups take a key of another type. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |
| `KeyEqual` | The equality of two keys, `bool operator()(const Key&, const Key&)`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `mapped_type` | `T` |
| `value_type` | `pair<const Key, T>` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `const value_type&` |
| `const_reference` | `const value_type&` |
| `pointer` | `const value_type*` |
| `const_pointer` | `const value_type*` |
| `const_iterator` | a forward iterator over `const value_type`, `std::forward_iterator` |
| `iterator` | `const_iterator` |
| [builder](map-builder.md) | a map changed in place and frozen into a map |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](map/map.md) | constructs the map |
| `(destructor)` | drops this version; the nodes no other version reaches are left to the collector |
| [operator=](map/operator_assign.md) | makes the variable hold another version |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](map/begin.md) | an iterator to the beginning |
| [end, cend](map/end.md) | an iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](map/empty.md) | checks whether the map is empty |
| [size](map/size.md) | the number of elements |

#### Lookup

| Function | Description |
|---|---|
| [at](map/at.md) | access the value under a key, with bounds checking |
| [count](map/count.md) | the number of elements under a key, 0 or 1 |
| [find](map/find.md) | the element under a key, as an iterator |
| [contains](map/contains.md) | checks whether the map has an element under a key |

#### New versions

| Function | Description |
|---|---|
| [insert](map/insert.md) | the map with an element added, when its key is absent |
| [set](map/set.md) | the map with a value under a key, added or in place of the value there |
| [update](map/update.md) | the map with a function of the value under a key in its place |
| [emplace](map/emplace.md) | the map with an element constructed in place, when its key is absent |
| [erase](map/erase.md) | the map without the element under a key |
| [thaw](map/thaw.md) | a builder over this map, for many changes at once |

#### Observers

| Function | Description |
|---|---|
| [hash_function](map/hash_function.md) | the hash of the keys |
| [key_eq](map/key_eq.md) | the equality of the keys |

#### From mixin::enumerable

The questions asked of the elements, the pairs, carried by every container of the library
([mixin::enumerable](../core/mixin/enumerable.md)); `contains` is the map's own, by the key.

| Function | Description |
|---|---|
| `index_of` | the position of the first element equal to a value |
| `last_index_of` | the position of the last element equal to a value |
| `find_if` | a pointer to the first element the predicate accepts |
| `find_index` | the position of the first element the predicate accepts |
| `exists` | checks whether the predicate accepts some element |
| `all` | checks whether the predicate accepts every element |
| `count_of` | the number of elements the predicate accepts |
| `min`, `max` | the smallest, the largest element |
| `for_each` | calls a function with every element |

#### From mixin::lookup

The reads of a map by its key ([mixin::lookup](../core/mixin/lookup.md)), through a pointer to the value rather
than an iterator, which carries a path of nodes.

| Function | Description |
|---|---|
| `get` | a copy of the value under a key, `nullopt` when absent |
| `try_get` | a pointer to the value under a key, null when absent |
| `value_or` | the value under a key, or a default |
| `contains_key` | checks whether the map has an element under a key |
| `keys` | the keys as a range, in the map's order |
| `values` | the values as a range, in the map's order |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](map/operator_cmp.md) | compare the elements |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>,
         class KeyEqual = std::equal_to<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>>>
map(InputIt, InputIt, Hash = Hash(), KeyEqual = KeyEqual())
    -> map<std::remove_const_t<typename std::iterator_traits<InputIt>::value_type::first_type>,
           typename std::iterator_traits<InputIt>::value_type::second_type, Hash, KeyEqual>;

template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
map(std::initializer_list<pair<const Key, T>>, Hash = Hash(), KeyEqual = KeyEqual())
    -> map<Key, T, Hash, KeyEqual>;
```

## Complexity

- Lookup (`find`, `at`, `contains`, `count`, `try_get`): logarithmic in the size, base 32, and one comparison of
  keys; one per element of a chain when hashes collide to the last bit.
- `insert`, `set`, `emplace`, `erase`: logarithmic in the size, base 32: the nodes on the path copied, up to 32
  elements each.
- The constructor from a range: O(*n* log *n*), the sort of the hashes; every node made once.
- Copy and assignment: constant, two words.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| every member but `operator=` | never: nothing changes the map |
| `operator=` | always |

An iterator holds the path from the root of the version it was taken from, as raw pointers: once the variable
holds another version, the nodes of the old one may be collected.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

// A configuration read on every request by many threads and changed once in a
// while by one: the readers take a snapshot, the writer publishes a new version
// that shares all but a path with the old one, and nobody copies a map or locks
int main() {
    concurrent::copy_on_write<immutable::map<string, int>> limits(
        immutable::map<string, int>{{"connections", 100}, {"requests", 1000}});
    atomic stop = false;
    atomic<long> reads = 0, inconsistent = 0;
    vector<thread> readers;
    for (int r : range(4)) {
        readers.emplace_back([&] {
            while (!stop) {
                auto snapshot = limits.load();  // one load: this version, while snapshot lives
                auto c = snapshot->try_get("connections");
                auto q = snapshot->try_get("requests");
                inconsistent += !c || !q || *q != 10 * *c;  // the writer keeps the ratio
                ++reads;
            }
        });
    }
    for (int i : range(1, 101)) {
        limits.update([i](auto& m) {  // two paths copied, the rest of the map shared
            m = m.set("connections", 100 * i).set("requests", 1000 * i);
        });
    }
    stop = true;
    for (auto& r : readers) {
        r.join();
    }
    auto last = limits.load();
    println("{} reads, {} inconsistent", reads.load(), inconsistent.load());
    println("connections {}, {} keys", last->at("connections"), last->size());
}
```

Sample output:

```text
1945 reads, 0 inconsistent
connections 10000, 2 keys
```

## See also

- [map::builder](map-builder.md): many changes at once
- [set](set.md): the same trie with the key as the element
- [vector](vector.md), [list](list.md): the immutable sequences
- [concurrent::copy_on_write](../concurrent/copy_on_write.md): how a version is published to other threads;
  [atomic](../core/atomic.md)
- [map](../core/map.md): the mutable one; [concurrent::map](../concurrent/map.md): the one many threads change in
  place
- [README: The rules](README.md#the-rules), [Benchmarks](benchmarks.md)
