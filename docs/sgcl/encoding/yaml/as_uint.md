[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::as_uint

```cpp
optional<uint64_t> as_uint() const noexcept;           // (1)
uint64_t as_uint(uint64_t fallback) const noexcept;    // (2)
```

The value of a non-negative integer, to 2^64 - 1; `nullopt` for a negative one, one past it and every other node.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `y["port"].as_uint(0)`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when there is no value |

## Return value

(1) The value, or `nullopt`; (2) the value, or `fallback`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::yaml::parse("[18446744073709551615, -1]").value();
    println("{} {}", v[0].as_uint(), v[1].as_uint());
    println(v[1].as_uint(0));
}
```

Output:

```text
18446744073709551615 nullopt
0
```

## See also

- [type](type.md)
- [sgcl::encoding::yaml](README.md)
