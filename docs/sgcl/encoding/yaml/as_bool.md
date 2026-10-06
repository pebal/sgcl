[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::as_bool

```cpp
optional<bool> as_bool() const noexcept;       // (1)
bool as_bool(bool fallback) const noexcept;    // (2)
```

The value of a boolean (`true`, `True`, `TRUE` and the falses); `nullopt` for every other node, YAML 1.1's `yes` among them.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `y["port"].as_bool(false)`.

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
    auto v = encoding::yaml::parse("[TRUE, false, yes]").value();
    println("{} {} {}", v[0].as_bool(), v[1].as_bool(), v[2].as_bool());
    println(v[2].as_bool(false));
}
```

Output:

```text
true false nullopt
false
```

## See also

- [type](type.md)
- [sgcl::encoding::yaml](README.md)
