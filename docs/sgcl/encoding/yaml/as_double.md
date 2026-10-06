[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::as_double

```cpp
optional<double> as_double() const noexcept;         // (1)
double as_double(double fallback) const noexcept;    // (2)
```

The value of a float (`.inf`, `-.inf`, `.nan` among them), or of an integer rounded to the nearest double; `nullopt` for every other node.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `y["port"].as_double(0.0)`.

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
    auto v = encoding::yaml::parse("[1.5e3, .inf, 7, '1.5']").value();
    for (const auto& e : v.elements()) {
        println(e.as_double());
    }
    println(v[3].as_double(0.0));
}
```

Output:

```text
1500
inf
7
nullopt
0
```

## See also

- [type](type.md)
- [sgcl::encoding::yaml](README.md)
