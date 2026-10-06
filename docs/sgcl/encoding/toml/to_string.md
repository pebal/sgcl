[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::to_string

```cpp
string to_string() const;
```

The table as a document, every line ended: its values first, `key = value`, then each table as `[a.b]` with its own
values and each array of tables as `[[a.b]]`, a blank line before each header; a table that holds nothing but tables
gets no header of its own, an empty one gets its header alone. Keys are bare when they may be (letters, digits, `_`,
`-`), quoted otherwise; strings in double quotes with escapes; arrays and the tables inside them inline. A value
read keeps its text (`0xFF`, `1_000`, `+inf`, `1979-05-27 07:32:00Z`).

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the value as written.

## Exceptions

`invalid_argument` for a value that is no table, or one nested deeper than 512 levels.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml v = encoding::toml::table({
        {"plain key", "text"}, {"lines", "first\nsecond"}, {"read", encoding::toml::parse("x = 0xFF")->operator[]("x")},
        {"owner", encoding::toml::table({{"name", "Tom"}})},
        {"servers", encoding::toml::table({{"alpha", encoding::toml::table({{"ip", "10.0.0.1"}})}})},
        {"product", encoding::toml::array({encoding::toml::table({{"sku", 1}}), encoding::toml::table({{"sku", 2}})})}});
    print(v.to_string());
}
```

Output:

```text
"plain key" = "text"
lines = "first\nsecond"
read = 0xFF

[owner]
name = "Tom"

[servers.alpha]
ip = "10.0.0.1"

[[product]]
sku = 1

[[product]]
sku = 2
```

## See also

- [parse](parse.md)
- [sgcl::encoding::toml](README.md)
