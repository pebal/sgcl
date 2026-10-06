[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::members

```cpp
slice<const member> members() const noexcept;
```

A table's [members](../toml-member.md) in the order their keys were first given, a slice of the value; empty for every other value.

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
    for (const auto& [key, value] : config["server"].members()) {
        println("{} = {}", key, value.text());
    }
}
```

Output:

```text
host = example.com
port = 8080
started = 2026-10-06T09:30:00+02:00
```

## See also

- [elements](elements.md)
- [sgcl::encoding::toml](README.md)
