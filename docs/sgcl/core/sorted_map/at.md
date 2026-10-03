[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::at

```cpp
mapped_type& at(const key_type& key);                           // (1)
const mapped_type& at(const key_type& key) const;               // (2)
template<class K> mapped_type& at(const K& key);                // (3)
template<class K> const mapped_type& at(const K& key) const;    // (4)
```

Returns a reference to the value under `key`, and throws when the map does not hold the key.

- (3–4) Take part only when `Compare` declares `is_transparent`: the key is of any type the comparison takes (a
  `std::string_view` or a literal for a `string` key), and no `Key` is built for the search.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

A reference to the mapped value of the element under `key`.

## Complexity

Logarithmic in the size of the map.

## Exceptions

- `out_of_range` when the map holds no element under `key`.
- (3–4) What the calls of `Compare` with a `K` throw; none when they are noexcept or `Compare` is a function object
  of `std`.

The map is not changed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
    ports.at("http") = 8080;  // a literal: no string is built for the search
    println("{} {}", ports.at("http"), ports.at("https"));

    try {
        ports.at("ftp");
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
8080 443
sgcl::sorted_map::at
```

## See also

- [operator[]](operator_at.md): the value under a key, inserted when absent
- [find](find.md): the element under a key, `end()` when absent
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
