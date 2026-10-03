[sgcl](../README.md) › [immutable](README.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>

```cpp
#include "sgcl/immutable/set.h"   // or "sgcl/immutable.h"

namespace sgcl::immutable {
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class set;
}
```

`sgcl::immutable::set<Key, Hash, KeyEqual>` is the immutable hash set: the hash array mapped trie of
[map](map.md), which has the account of the structure, with the key as the element. Every `insert` and `erase`
returns a new set that shares all but the path it changed with the old one, which stays exactly as it was; a
lookup walks log32(*n*) nodes and compares the key once.

The set is two words and its function objects, the size and a `tracked_ptr` to the root, and a copy of it is a
copy of those words. The nodes are managed objects that no version owns, collected once the last version that
reaches them is dropped.

What differs from `std::unordered_set` and from a Go map used as a set: nothing is ever modified, so a set held by
any number of threads is read by all of them without a lock, and a version is published, and replaced by the
next, through a [concurrent::copy_on_write](../concurrent/copy_on_write.md) or an [atomic](../core/atomic.md).
Many changes at once go through a [builder](set-builder.md).

## Rules

- A set holds its root by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed
  object ([The rules](../core/README.md#the-rules), 1).
- Every member but the assignment is `const`. `insert` and `erase` return the new set; the one they were called
  on is unchanged, and stays so for as long as it is held. The elements are `const`.
- A change copies the elements of the nodes on the path, up to 32 per node: the copy constructor of `Key` is what
  a change costs, plus the nodes.
- An element holding tracked pointers is traced where it lives, in a node; one with a destructor is destroyed
  when the collector frees its node, once no version reaches it.
- Its iterators hold nothing alive: valid while the set object they came from exists and holds the same version.
- With a transparent hash and equality (`is_transparent`, as those of a [string](../core/string.md) are) the
  lookups and `erase` take a key of another type and build none: a `string_view` or a literal for a `string`.
- Sharing between threads is as for the [map](map.md#rules): any number of threads read any version; the
  variable one thread replaces is published through
  [concurrent::copy_on_write](../concurrent/copy_on_write.md), an update costing a path.
- The order of iteration is the trie's, the bits of the hashes; it changes with nothing but the elements.

## Template parameters

| Parameter | Description |
|---|---|
| `Key` | The type of the elements. A change copies the elements of the nodes on its path, so it requires `Key` to be copy constructible. |
| `Hash` | The hash of an element, `size_t operator()(const Key&)`. With a member type `is_transparent`, together with `KeyEqual`'s, the lookups take a key of another type. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |
| `KeyEqual` | The equality of two elements, `bool operator()(const Key&, const Key&)`. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `key_type` | `Key` |
| `value_type` | `Key` |
| `hasher` | `Hash` |
| `key_equal` | `KeyEqual` |
| `size_type` | `size_t` |
| `difference_type` | `ptrdiff_t` |
| `reference` | `const Key&` |
| `const_reference` | `const Key&` |
| `pointer` | `const Key*` |
| `const_pointer` | `const Key*` |
| `const_iterator` | a forward iterator over `const Key`, `std::forward_iterator` |
| `iterator` | `const_iterator` |
| [builder](set-builder.md) | a set changed in place and frozen into a set |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](set/set.md) | constructs the set |
| `(destructor)` | drops this version; the nodes no other version reaches are left to the collector |
| [operator=](set/operator_assign.md) | makes the variable hold another version |

#### Iterators

| Function | Description |
|---|---|
| [begin, cbegin](set/begin.md) | an iterator to the beginning |
| [end, cend](set/end.md) | an iterator to the end |

#### Capacity

| Function | Description |
|---|---|
| [empty](set/empty.md) | checks whether the set is empty |
| [size](set/size.md) | the number of elements |

#### Lookup

| Function | Description |
|---|---|
| [count](set/count.md) | the number of elements equal to a key, 0 or 1 |
| [find](set/find.md) | the element equal to a key, as an iterator |
| [contains](set/contains.md) | checks whether the set has an element equal to a key |

#### New versions

| Function | Description |
|---|---|
| [insert](set/insert.md) | the set with an element added, when it is absent |
| [erase](set/erase.md) | the set without an element |
| [thaw](set/thaw.md) | a builder over this set, for many changes at once |

#### Observers

| Function | Description |
|---|---|
| [hash_function](set/hash_function.md) | the hash of the elements |
| [key_eq](set/key_eq.md) | the equality of the elements |

#### From mixin::enumerable

The questions asked of the elements, carried by every container of the library
([mixin::enumerable](../core/mixin/enumerable.md)); `contains` is the set's own, by the key.

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

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](set/operator_cmp.md) | compare the elements |

## Deduction guides

```cpp
template<std::input_iterator InputIt,
         class Hash = std::hash<typename std::iterator_traits<InputIt>::value_type>,
         class KeyEqual = std::equal_to<typename std::iterator_traits<InputIt>::value_type>>
set(InputIt, InputIt, Hash = Hash(), KeyEqual = KeyEqual())
    -> set<typename std::iterator_traits<InputIt>::value_type, Hash, KeyEqual>;

template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
set(std::initializer_list<Key>, Hash = Hash(), KeyEqual = KeyEqual())
    -> set<Key, Hash, KeyEqual>;
```

## Complexity

- Lookup (`find`, `contains`, `count`): logarithmic in the size, base 32, and one comparison of keys; one per
  element of a chain when hashes collide to the last bit.
- `insert`, `erase`: logarithmic in the size, base 32: the nodes on the path copied, up to 32 elements each.
- The constructor from a range: O(*n* log *n*), the sort of the hashes; every node made once.
- Copy and assignment: constant, two words.

## Iterator invalidation

| Operations | Invalidated |
|---|---|
| every member but `operator=` | never: nothing changes the set |
| `operator=` | always |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

// The ids seen so far, one version per step: what was seen at step k is still
// known at the end, and the versions share all but a path each
int main() {
    vector<immutable::set<int>> at_step;
    immutable::set<int> seen;
    for (int id : {7, 3, 7, 9, 3, 1}) {
        seen = seen.insert(id);
        at_step.push_back(seen);  // two words: the version as it is now
    }
    for (int k : range(at_step.size())) {
        const auto& s = at_step[k];
        println("step {}: {} ids, 9 {}", k, s.size(), s.contains(9) ? "seen" : "not yet");
    }
}
```

Output:

```text
step 0: 1 ids, 9 not yet
step 1: 2 ids, 9 not yet
step 2: 2 ids, 9 not yet
step 3: 3 ids, 9 seen
step 4: 3 ids, 9 seen
step 5: 4 ids, 9 seen
```

## See also

- [set::builder](set-builder.md): many changes at once
- [map](map.md): the structure, and the same trie with a value per key
- [vector](vector.md), [list](list.md): the immutable sequences
- [concurrent::copy_on_write](../concurrent/copy_on_write.md): how a version is published to other threads
- [set](../core/set.md): the mutable one; [concurrent::set](../concurrent/set.md): the one many threads change in
  place
- [README: The rules](README.md#the-rules)
