[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_uint

```cpp
optional<uint64_t> as_uint() const noexcept;           // (1)
uint64_t as_uint(uint64_t fallback) const noexcept;    // (2)
```

The value of a non-negative integer, to 2^64 - 1; `nullopt` for a negative one and every other kind.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `c["n"].as_uint(0)`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when there is no value |

## Return value

(1) The value, or `nullopt`; (2) the value, or `fallback`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::cbor(uint64_t(-1)).as_uint());
    println(encoding::cbor(-1).as_uint());
    println(encoding::cbor(-1).as_uint(0));
}
```

Output:

```text
18446744073709551615
nullopt
0
```

## See also

- [as_int](as_int.md)
- [sgcl::encoding::cbor](README.md)
