[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::set

```cpp
set();                                                                 // (1)
explicit set(const Hash& hash, const KeyEqual& equal = KeyEqual());    // (2)
template<std::input_iterator InputIt>
set(InputIt first, InputIt last,                                       // (3)
    const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
    noexcept(/* see below */);
set(std::initializer_list<value_type> ilist,                           // (4)
    const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
    noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
             std::is_nothrow_move_constructible_v<value_type>);
set(const set& other) noexcept;                                        // (5)
set(set&& other) noexcept;                                             // (6)
```

Constructs a set from one of the sources below.

1. An empty set. It holds no node at all.
2. An empty set with the hash and the equality given.
3. The elements of the range `[first, last)`, built at once.
4. The elements of `ilist`, built at once.
5. The version `other` holds: a copy of its two words and its function objects, every node shared.
6. The same as (5): `other` keeps its version too.

A range or a list (3–4) is built at once, as the [map's](../map/map.md) is: the elements sorted by their hashes in
the trie's order and the trie made from the root down, every node allocated once. An element that occurs twice is
kept once, its first occurrence.

## Parameters

| Parameter | Description |
|---|---|
| `hash` | the hash of the elements |
| `equal` | the equality of the elements |
| `first`, `last` | the range of elements |
| `ilist` | the list of elements |
| `other` | the set whose version is taken |

## Complexity

- (1–2) Constant.
- (3–4) O(*n* log *n*) for the sort of the hashes, *n* the number of elements given; every node made once.
- (5–6) Constant.

## Exceptions

- (1) None, unless the default constructor of `Hash` or `KeyEqual` throws.
- (2) What the copy of `hash` and `equal` throws.
- (3) What the walk over the range (the copy, the comparison, the increment and the dereference of
  `InputIt`), the construction of an element from `*first` and the move of an element throw; none when they
  are noexcept.
- (4) What the copy and the move of an element throw; none when they are noexcept.
- (5–6) None.

When an exception is thrown, what was made is left to the collector and no set is made.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<string> names = {"alice", "bob", "alice"};
    println("{} {}", names.size(), names.contains("bob"));

    vector<int> ids = {3, 1, 3, 2};
    immutable::set from_range(ids.begin(), ids.end());  // deduced: set<int>
    immutable::set copy = from_range;  // two words: the nodes are shared
    println("{} {}", copy.size(), copy == from_range);
}
```

Output:

```text
2 true
3 true
```

## See also

- [insert](insert.md): the set with one more element
- [thaw](thaw.md): a builder, for a set made one element at a time
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
