[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::find

```cpp
iterator find(const key_type& key) noexcept;                                            // (1)
const_iterator find(const key_type& key) const noexcept;                                // (2)
template<class K> iterator find(const K& key) noexcept(/* see below */);                // (3)
template<class K> const_iterator find(const K& key) const noexcept(/* see below */);    // (4)
```

Finds the element under `key`: the walk of the key's bucket, the cached hash of each node compared before its
key, reading raw pointers only.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../string.md) do: a `string_view`,
  a [string_slice](../string.md) or a literal finds a `string` key with no string made for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element to find |

## Return value

An iterator to the element, or [end()](end.md) when the key is not there.

## Complexity

Constant on average, linear in the size when every key falls into one bucket.

## Exceptions

- (1–2) None.
- (3–4) None when the calls of `Hash` and `KeyEqual` with a `K` are noexcept or they are the function objects of
  `std`; otherwise what they throw.

## Notes

A lookup pays no write barrier and constructs no `tracked_ptr`: the map roots every node it links.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> m = {{"apple", 1}, {"pear", 2}};
    auto it = m.find("apple");  // a literal: no string is built
    println("{} {}", it->first, it->second);

    string line = "pear pie";
    string_slice key = line.as_slice(0, 4);  // a piece of another string
    println("{} {}", m.find(key)->second, m.find("plum") == m.end());
}
```

Output:

```text
apple 1
2 true
```

## See also

- [contains](contains.md): checks whether a key is there
- [at](at.md): the value under a key, with bounds checking
- [equal_range](equal_range.md): the range of the elements under a key
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
