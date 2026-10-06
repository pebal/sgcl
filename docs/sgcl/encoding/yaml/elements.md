[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::elements

```cpp
slice<const yaml> elements() const noexcept;
```

A sequence's elements, a slice of the node, nothing copied; empty for every other node.

## Parameters

None.

## Return value

The elements.

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
    encoding::yaml config = encoding::yaml::parse(R"(
server:
  host: example.com
  port: 8080
  tls: true
paths: [/a, /b]
)").value();
    for (const auto& p : config["paths"].elements()) {
        println(p.as_string("?"));
    }
}
```

Output:

```text
/a
/b
```

## See also

- [members](members.md)
- [sgcl::encoding::yaml](README.md)
