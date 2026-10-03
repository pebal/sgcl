[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::map

```cpp
/*(1)*/ map();
/*(2)*/ explicit map(const Hash& hash, const KeyEqual& equal = KeyEqual());
/*(3)*/ template<std::input_iterator InputIt>
        map(InputIt first, InputIt last,
            const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
            noexcept(/* see below */);
/*(4)*/ map(std::initializer_list<value_type> ilist,
            const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
            noexcept(std::is_nothrow_copy_constructible_v<value_type> &&
                     std::is_nothrow_move_constructible_v<value_type>);
/*(5)*/ map(const map& other) noexcept;
/*(6)*/ map(map&& other) noexcept;
```

Constructs a map from one of the sources below.

1. An empty map. It holds no node at all.
2. An empty map with the hash and the equality given.
3. The elements of the range `[first, last)`, built at once.
4. The elements of `ilist`, built at once.
5. The version `other` holds: a copy of its two words and its function objects, every node shared.
6. The same as (5): `other` keeps its version too.

A range or a list (3–4) is built at once, not by an insert per element: the elements are taken into a buffer,
sorted by their hashes in the trie's order (the low chunk first, as the trie consumes it), and the trie is made
from the root down, every node allocated once at its size and every entry constructed once. A key that occurs
twice keeps its first occurrence, as an insert keeps what it finds and as a `std::unordered_map` made of the
range does.

## Parameters

| Parameter | Description |
|---|---|
| `hash` | the hash of the keys |
| `equal` | the equality of the keys |
| `first`, `last` | the range of elements, pairs of a key and a value |
| `ilist` | the list of elements |
| `other` | the map whose version is taken |

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

When an exception is thrown, what was made is left to the collector and no map is made.

## Notes

90 ns per element for 200,000 random `long` keys, of which the sort is the larger part, against 700 ns for the
inserts that would build the same map ([Benchmarks](../benchmarks.md#against-std)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> ports = {{"http", 80}, {"https", 443}, {"http", 8080}};
    println("{} keys, http {}", ports.size(), ports.at("http"));

    sorted_map<int, int> squares = {{1, 1}, {2, 4}, {3, 9}};
    immutable::map from_range(squares.begin(), squares.end());  // deduced: map<int, int>
    println("{} {}", from_range.size(), from_range.at(3));

    immutable::map copy = ports;  // two words: the nodes are shared
    println("{}", copy == ports);
}
```

Output:

```text
2 keys, http 80
3 9
true
```

## See also

- [operator=](operator_assign.md): makes the variable hold another version
- [insert](insert.md), [set](set.md): the map with one more element
- [thaw](thaw.md): a builder, for a map made one element at a time
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
