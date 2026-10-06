[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::set

```cpp
cbor set(const cbor& key, const cbor& value) const noexcept;
```

A new value: a map with the key set to the value, in its place when the key is there, at the end when it is not;
an array with the element at an integer index replaced, or added at one past the end. The value itself never
changes; anything else, an index further out among them, gives the value as it is.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, or the index of an array |
| `value` | the value |

## Return value

The new value.

## Complexity

Linear in the size of the map or the array.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor m = encoding::cbor::map({{"a", 1}});
    println(m.set("a", 2).set("b", 3).to_string());
    println(encoding::cbor::array({1, 2}).set(2, 3).to_string());
    println(m.to_string());
}
```

Output:

```text
{"a": 2, "b": 3}
[1, 2, 3]
{"a": 1}
```

## See also

- [erase](erase.md)
- [push_back](push_back.md)
- [sgcl::encoding::cbor](README.md)
