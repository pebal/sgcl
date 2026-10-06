[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::as_bool

```cpp
optional<bool> as_bool() const noexcept;       // (1)
bool as_bool(bool fallback) const noexcept;    // (2)
```

The value of a boolean; `nullopt` for every other value.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `t["port"].as_bool(false)`.

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
    auto v = encoding::toml::parse("a = true\nb = 'true'").value();
    println("{} {}", v["a"].as_bool(), v["b"].as_bool());
    println(v["b"].as_bool(false));
}
```

Output:

```text
true nullopt
false
```

## See also

- [type](type.md)
- [sgcl::encoding::toml](README.md)
