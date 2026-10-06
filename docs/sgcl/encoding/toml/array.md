[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::array

```cpp
static toml array(std::initializer_list<toml> elements) noexcept;    // (1)
static toml array(const vector<toml>& elements) noexcept;            // (2)
```

An array of the elements in their order, of any kinds (TOML 1.0 lets an array mix them). An array of nothing but
tables is written as an array of tables, `[[name]]`, the others inline.

1. Of a list written out.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the elements |

## Return value

The array.

## Complexity

Linear in the count of the elements.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<encoding::toml> servers;
    for (int i : range(1, 3)) {
        servers.push_back(encoding::toml::table({{"name", string("node" + std::to_string(i))}}));
    }
    print(encoding::toml::table({{"ports", encoding::toml::array({80, 443})},
                                 {"server", encoding::toml::array(servers)}}).to_string());
}
```

Output:

```text
ports = [80, 443]

[[server]]
name = "node1"

[[server]]
name = "node2"
```

## See also

- [table](table.md)
- [sgcl::encoding::toml](README.md)
