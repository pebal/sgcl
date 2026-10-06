[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::table

```cpp
static toml table(std::initializer_list<member> members) noexcept;    // (1)
static toml table(const vector<member>& members) noexcept;            // (2)
```

A table of the members in their order. A key given twice is kept twice here; written and read back it is a
[parse](parse.md) error, so a program gives every key once ([set](set.md) replaces a key's value).

1. Of a list written out, `{key, value}` each.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `members` | the keys and their values |

## Return value

The table.

## Complexity

Linear in the count of the members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml v = encoding::toml::table({
        {"name", "app"},
        {"database", encoding::toml::table({{"host", "db.local"}, {"pool", 8}})}});
    print(v.to_string());
}
```

Output:

```text
name = "app"

[database]
host = "db.local"
pool = 8
```

## See also

- [array](array.md)
- [sgcl::encoding::toml](README.md)
