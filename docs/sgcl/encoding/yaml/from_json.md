[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::from_json

```cpp
static yaml from_json(const json& j) noexcept;
```

A [json](../json/README.md) value as YAML: null, a boolean, an integer or a float (a number kept as its literal
keeps it), a string, a sequence, a mapping of string keys in their order.

## Parameters

| Parameter | Description |
|---|---|
| `j` | the JSON value |

## Return value

The node.

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
    auto j = encoding::json::parse(R"({"name": "app", "ports": [80, 443], "debug": false})").value();
    print(encoding::yaml::from_json(j).to_string());
}
```

Output:

```text
name: app
ports:
  - 80
  - 443
debug: false
```

## See also

- [to_json](to_json.md)
- [sgcl::encoding::yaml](README.md)
