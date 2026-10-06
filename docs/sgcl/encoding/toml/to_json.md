[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::to_json

```cpp
json to_json() const noexcept;
```

The value as [json](../json/README.md): strings, integers, floats and booleans as they are (`inf` and `nan` null),
a date or a time its TOML text, arrays and tables in their order.

## Parameters

None.

## Return value

The JSON value.

## Complexity

Linear in the size of the value.

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
    println(config.to_json().to_string());
}
```

Output:

```text
{"title":"app","server":{"host":"example.com","port":8080,"started":"2026-10-06T09:30:00+02:00"},"route":[{"path":"/a"},{"path":"/b"}]}
```

## See also

- [from_json](from_json.md)
- [sgcl::encoding::toml](README.md)
