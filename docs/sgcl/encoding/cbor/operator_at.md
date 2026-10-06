[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::operator[]

```cpp
template<class I> cbor operator[](I index_or_key) const noexcept;           // (1)
cbor operator[](const string& key) const noexcept;                          // (2)
template<size_t N> cbor operator[](const char (&key)[N]) const noexcept;
cbor operator[](const cbor& key) const noexcept;                            // (3)
```

A value inside an array or a map, a copy of the handle; null when there is none (and for a value of another kind).

1. An integer, of any integral type but `bool`: an array's element at the index (null for a negative one or past
   the end), a map's value of that integer key — so `key[-1]` reads COSE's keys.
2. A map's value of the text key; the literal's overload makes `m["key"]` unambiguous.
3. A map's value of the key of any kind, by deep equality.

## Parameters

| Parameter | Description |
|---|---|
| `index_or_key` | the index or the integer key |
| `key` | the key |

## Return value

The value, or null.

## Complexity

Linear in the members of a map; constant for an array.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor key = encoding::cbor::map({{1, 2}, {-1, 1}, {"kid", "k1"}});
    encoding::cbor list = encoding::cbor::array({10, 20});
    println("{} {} {}", *key[1].as_int(), *key[-1].as_int(), *key["kid"].as_string());
    println("{} {}", *list[1].as_int(), list[5].to_string());
}
```

Output:

```text
2 1 k1
20 null
```

## See also

- [contains](contains.md)
- [elements](elements.md), [members](members.md)
- [sgcl::encoding::cbor](README.md)
