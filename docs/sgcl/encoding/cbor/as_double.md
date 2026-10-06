[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::as_double

```cpp
optional<double> as_double() const noexcept;         // (1)
double as_double(double fallback) const noexcept;    // (2)
```

The value of a float, or of an integer rounded to the nearest double; `nullopt` for every other kind.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `c["n"].as_double(0.0)`.

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
    println(encoding::cbor(1.5).as_double());
    println(encoding::cbor(uint64_t(-1)).as_double());
    println(encoding::cbor("1.5").as_double());
    println(encoding::cbor("1.5").as_double(0.0));
}
```

Output:

```text
1.5
18446744073709551616
nullopt
0
```

## See also

- [as_int](as_int.md)
- [sgcl::encoding::cbor](README.md)
