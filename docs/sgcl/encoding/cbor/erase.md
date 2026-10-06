[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::erase

```cpp
cbor erase(const cbor& key) const noexcept;
```

A new value without the key of a map, or without the element at an integer index of an array; the value as it is
when there is none.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, or the index |

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
    println(encoding::cbor::map({{"a", 1}, {"b", 2}}).erase("a").to_string());
    println(encoding::cbor::array({1, 2, 3}).erase(0).to_string());
}
```

Output:

```text
{"b": 2}
[2, 3]
```

## See also

- [set](set.md)
- [sgcl::encoding::cbor](README.md)
