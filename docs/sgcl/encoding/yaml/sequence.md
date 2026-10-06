[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::sequence

```cpp
static yaml sequence(std::initializer_list<yaml> elements) noexcept;    // (1)
static yaml sequence(const vector<yaml>& elements) noexcept;            // (2)
```

A sequence of the elements in their order.

1. Of a list written out.
2. Of a [vector](../../core/vector/README.md) made in a loop.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the elements |

## Return value

The node.

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
    vector<encoding::yaml> hosts;
    for (int i : range(1, 4)) {
        hosts.push_back(string("node" + std::to_string(i)));
    }
    print(encoding::yaml::mapping({{"hosts", encoding::yaml::sequence(hosts)}}).to_string());
}
```

Output:

```text
hosts:
  - node1
  - node2
  - node3
```

## See also

- [mapping](mapping.md)
- [sgcl::encoding::yaml](README.md)
