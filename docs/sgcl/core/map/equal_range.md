[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::equal_range

```cpp
/*(1)*/ std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;
/*(2)*/ std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;
/*(3)*/ template<class K>
        std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);
/*(4)*/ template<class K>
        std::pair<const_iterator, const_iterator> equal_range(const K& key) const
            noexcept(/* see below */);
```

Returns the range of the elements under `key`: in a map, the element alone or nothing.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements |

## Return value

A pair of iterators: the element and the one after it in the iteration, or two `end()` iterators when the key is
not there.

## Complexity

Constant on average, linear in the size when every key falls into one bucket.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what they throw.

## Notes

`equal_range` is there for code written for a [multimap](../multimap.md) as well; it also gives a map
`values_of` of [mixin::lookup](../mixin/lookup.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> m = {{"a", 1}, {"b", 2}};
    auto [from, to] = m.equal_range("a");
    println("{} {}", std::distance(from, to), from->second);

    auto [none, end] = m.equal_range("z");
    println("{} {}", none == end, none == m.end());
}
```

Output:

```text
1 1
true true
```

## See also

- [find](find.md): an iterator to the element under a key
- [count](count.md): the number of elements under a key
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
