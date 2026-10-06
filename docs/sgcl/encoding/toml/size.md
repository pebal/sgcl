[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::size

```cpp
size_t size() const noexcept;
```

The elements of an array, the members of a table; 0 for a scalar.

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
    encoding::toml config = encoding::toml::parse(R"(
title = "app"

[server]
host = "example.com"
port = 8080
started = 2026-10-06T09:30:00+02:00

[[route]]
path = "/a"

[[route]]
path = "/b"
)").value();
    println("{} {} {}", config.size(), config["route"].size(), config["title"].size());
}
```

Output:

```text
3 2 0
```

## See also

- [empty](empty.md)
- [sgcl::encoding::toml](README.md)
