[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::equal_range

```cpp
std::pair<iterator, iterator> equal_range(const key_type& key) noexcept;                      // (1)
std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const noexcept;    // (2)
template<class K>
std::pair<iterator, iterator> equal_range(const K& key) noexcept(/* see below */);            // (3)
template<class K>
std::pair<const_iterator, const_iterator> equal_range(const K& key) const                     // (4)
    noexcept(/* see below */);
```

Returns the range of the elements under `key`: the element and the one after it in the order when the key is
there, an empty range otherwise. The keys being unique, the range holds one element or none.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the elements |

## Return value

A pair of iterators: the element and the next in the order (`end()` after the newest), or `end()` twice when the
key is not there.

## Complexity

Constant on average, linear in `size()` in the worst case.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what those calls throw.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<string, int> m = {{"c", 1}, {"a", 2}, {"b", 3}};
    auto [first, last] = m.equal_range("a");
    println("{} {}", first->first, last->first);

    auto [none, end] = m.equal_range("z");
    println("{} {}", none == m.end(), end == m.end());
}
```

Output:

```text
a b
true true
```

## See also

- [find](find.md): an iterator to the element under a key
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
