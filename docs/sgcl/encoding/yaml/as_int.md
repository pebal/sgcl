[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::as_int

```cpp
optional<int64_t> as_int() const noexcept;          // (1)
int64_t as_int(int64_t fallback) const noexcept;    // (2)
```

The value of an integer an `int64_t` holds, in decimal, octal (`0o`) or hexadecimal (`0x`); `nullopt` for one past it, a float and every other node.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `y["port"].as_int(80)`.

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
    auto v = encoding::yaml::parse("[0o17, 0x1F, -12, 18446744073709551615, 1.0]").value();
    for (const auto& e : v.elements()) {
        println(e.as_int());
    }
    println(v[4].as_int(-1));
}
```

Output:

```text
15
31
-12
nullopt
nullopt
-1
```

## See also

- [type](type.md)
- [sgcl::encoding::yaml](README.md)
