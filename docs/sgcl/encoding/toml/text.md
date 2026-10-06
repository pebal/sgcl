[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::text

```cpp
string text() const noexcept;
```

A scalar's text as it was written: `0xFF` of an integer, `1e3` of a float, `1979-05-27 07:32:00Z` of a date-time,
a string's characters (its escapes read); what a constructor wrote for a scalar made here. Empty for an array and a
table.

## Parameters

None.

## Return value

The text.

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
    auto v = encoding::toml::parse("a = [0xFF, 1e3, +inf, 1979-05-27 07:32:00Z, 'x', []]").value();
    for (const auto& e : v["a"].elements()) {
        println("[{}] {}", e.text(), e.to_json().to_string());
    }
}
```

Output:

```text
[0xFF] 255
[1e3] 1000
[+inf] null
[1979-05-27 07:32:00Z] "1979-05-27 07:32:00Z"
[x] "x"
[] []
```

## See also

- [as_string](as_string.md)
- [sgcl::encoding::toml](README.md)
