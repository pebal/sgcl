[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::size

```cpp
size_t size() const noexcept;
```

The elements of a sequence, the members of a mapping; 0 for a scalar.

## Parameters

None.

## Return value

The count.

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
    println("{} {} {}", config.size(), config["paths"].size(), config["server"]["host"].size());
}
```

Output:

```text
2 2 0
```

## See also

- [empty](empty.md)
- [sgcl::encoding::yaml](README.md)
