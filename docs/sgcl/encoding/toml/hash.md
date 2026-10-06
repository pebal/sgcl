[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::hash

```cpp
size_t hash() const noexcept;
```

A hash of the value, equal for [equal](operator_cmp.md) values (`0x10` and `16` alike, a table's members in any
order, an offset date-time by its instant): what `std::hash<encoding::toml>` gives, so a value is a key of a
[map](../../core/map/README.md).

## Parameters

None.

## Return value

The hash.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto p = [](const char* t) { return encoding::toml::parse(t).value(); };
    println(p("a = 0x10").hash() == p("a = 16").hash());
    println(p("a = 1\nb = 2").hash() == p("b = 2\na = 1").hash());
}
```

Output:

```text
true
true
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::encoding::toml](README.md)
