[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::contains

```cpp
bool contains(const string& key) const noexcept;
```

Whether a table has the key: what [operator[]](operator_at.md) cannot tell from a key whose value is an empty
table.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

`true` when the table has it.

## Complexity

Linear in the members.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("[a]\n[b]\nx = 1").value();
    println("{} {} {} {}", v.contains("a"), v.contains("c"), v["a"].empty(), v["c"].empty());
}
```

Output:

```text
true false true true
```

## See also

- [operator\\[\\]](operator_at.md)
- [sgcl::encoding::toml](README.md)
