[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::members

```cpp
slice<const member> members() const noexcept;
```

A mapping's [members](../yaml-member.md) in their order, a slice of the node; empty for every other node.

## Parameters

None.

## Return value

The members.

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
    for (const auto& [key, value] : config["server"].members()) {
        println("{} = {}", key.text(), value.text());
    }
}
```

Output:

```text
host = example.com
port = 8080
tls = true
```

## See also

- [elements](elements.md)
- [sgcl::encoding::yaml](README.md)
