[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_int

```cpp
optional<int64_t> as_int() const noexcept;          // (1)
int64_t as_int(int64_t fallback) const noexcept;    // (2)
```

The value of an integer an `int64_t` holds; `nullopt` for one past it (to 2^64 - 1 [as_uint](as_uint.md), any
[as_big_integer](as_big_integer.md)), for a float and every other kind.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `c["n"].as_int(0)`.

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
    println(encoding::cbor(-1000).as_int());
    println(encoding::cbor(uint64_t(-1)).as_int());
    println(encoding::cbor(1.0).as_int());
    println(encoding::cbor("7").as_int(-1));
}
```

Output:

```text
-1000
nullopt
nullopt
-1
```

## See also

- [as_uint](as_uint.md)
- [sgcl::encoding::cbor](README.md)
