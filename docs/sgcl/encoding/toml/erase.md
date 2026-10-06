[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::erase

```cpp
toml erase(const string& key) const noexcept;
```

A new table without the key; the value as it is when there is none.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

The new value.

## Complexity

Linear in the size of the table.

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
    print(config.erase("server").erase("missing").erase("route").to_string());
}
```

Output:

```text
title = "app"
```

## See also

- [set](set.md)
- [sgcl::encoding::toml](README.md)
