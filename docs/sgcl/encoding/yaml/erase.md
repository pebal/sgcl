[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::erase

```cpp
yaml erase(const yaml& key) const noexcept;
```

A new node without the key of a mapping, or without the element at an integer index of a sequence; the node as it is
when there is none.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key, or the index |

## Return value

The new node.

## Complexity

Linear in the size of the mapping or the sequence.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml config = encoding::yaml::parse(R"(
server:
  host: example.com
  port: 8080
  tls: true
paths: [/a, /b]
)").value();
    print(config.erase("server").erase("missing").to_string());
}
```

Output:

```text
paths:
  - /a
  - /b
```

## See also

- [set](set.md)
- [sgcl::encoding::yaml](README.md)
