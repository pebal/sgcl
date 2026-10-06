[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::elements

```cpp
slice<const toml> elements() const noexcept;
```

An array's elements, a slice of the value, nothing copied; empty for every other value.

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
    for (const auto& r : config["route"].elements()) {
        println(r["path"].as_string("?"));
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
- [sgcl::encoding::toml](README.md)
