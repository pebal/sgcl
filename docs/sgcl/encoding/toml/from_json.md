[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::from_json

```cpp
static toml from_json(const json& j) noexcept;
```

A [json](../json/README.md) value as TOML: a boolean, an integer when the number is one within 64 bits, a float
otherwise, a string, an array, a table of string keys in their order. Null has no TOML: it is left out of a table
and an array, and a null alone gives an empty table.

## Parameters

| Parameter | Description |
|---|---|
| `j` | the JSON value |

## Return value

The value.

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
    auto j = encoding::json::parse(R"({"name": "app", "ports": [80, 443], "proxy": null, "tls": {"on": true}})").value();
    print(encoding::toml::from_json(j).to_string());
}
```

Output:

```text
name = "app"
ports = [80, 443]

[tls]
on = true
```

## See also

- [to_json](to_json.md)
- [sgcl::encoding::toml](README.md)
