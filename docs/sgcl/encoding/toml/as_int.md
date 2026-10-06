[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::as_int

```cpp
optional<int64_t> as_int() const noexcept;          // (1)
int64_t as_int(int64_t fallback) const noexcept;    // (2)
```

The value of an integer, in any of its bases and with its underscores; `nullopt` for a float and every other value.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `t["port"].as_int(80)`.

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
    auto v = encoding::toml::parse("a = [0xFF, 0o17, 0b101, 1_000, -12, 1.0]").value();
    for (const auto& e : v["a"].elements()) {
        println(e.as_int());
    }
    println(v["a"][5].as_int(-1));
}
```

Output:

```text
255
15
5
1000
-12
nullopt
-1
```

## See also

- [type](type.md)
- [sgcl::encoding::toml](README.md)
