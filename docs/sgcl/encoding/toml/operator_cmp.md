[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::operator== (sgcl::encoding::toml)

```cpp
friend bool operator==(const toml& a, const toml& b) noexcept;
```

Deep equality; `!=` is made from it by the compiler. Values of different kinds are never equal (`1` and `1.0`, an
offset and a local date-time); scalars by the value read (`0x10` is `16`, `1e1` is `10.0`, `0.0` is `-0.0`, every
`nan` one value, an offset date-time by its instant, a local one by its fields); strings by their characters; arrays
element by element; tables as sets of members, in any order.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the values.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto p = [](const char* t) { return encoding::toml::parse(t).value(); };
    println("{} {} {} {}", p("a = 0x10") == p("a = 16"), p("a = 1") == p("a = 1.0"),
            p("a = 1979-05-27T07:32:00-08:00") == p("a = 1979-05-27T15:32:00Z"), p("a = 1\nb = 2") == p("b = 2\na = 1"));
}
```

Output:

```text
true false true true
```

## See also

- [hash](hash.md)
- [sgcl::encoding::toml](README.md)
