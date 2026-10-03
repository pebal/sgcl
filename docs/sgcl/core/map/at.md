[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::at

```cpp
/*(1)*/ mapped_type& at(const key_type& key);
/*(2)*/ const mapped_type& at(const key_type& key) const;
/*(3)*/ template<class K> mapped_type& at(const K& key);
/*(4)*/ template<class K> const mapped_type& at(const K& key) const;
```

Returns a reference to the value under `key`, with bounds checking: a key that is not in the map throws.

- (1–2) The key is of the key type.
- (3–4) The key is of any type the hash and the equality take. Take part only when `Hash` and `KeyEqual` both
  declare `is_transparent`, as `std::hash` and `std::equal_to` of a [string](../string.md) do: a `string_view`
  or a literal finds a `string` key with no string made for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

A reference to the value under the key.

## Complexity

Constant on average, linear in the size when every key falls into one bucket.

## Exceptions

`out_of_range` when the key is not in the map, and (3–4) what the calls of `Hash` and `KeyEqual` with a `K`
throw; none from them when they are noexcept or the function objects of `std`.

## Notes

[operator[]](operator_at.md) inserts the key instead of throwing; `get`, `try_get` and `value_or` of
[mixin::lookup](../mixin/lookup.md) answer an absent key without an exception. The reference is valid while the
element is in the map.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> ports = {{"http", 80}, {"ssh", 22}};
    ports.at("ssh") = 2222;
    println("{} {}", ports.at("http"), ports.at("ssh"));

    try {
        ports.at("ftp") = 21;
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
80 2222
out of range: sgcl::map::at
```

## See also

- [operator[]](operator_at.md): the value under a key, inserted when absent
- [find](find.md): an iterator to the element under a key
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
