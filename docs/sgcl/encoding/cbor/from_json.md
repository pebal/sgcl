[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::from_json

```cpp
static cbor from_json(const json& j) noexcept;
```

A [json](../json/README.md) value as CBOR (§6.2): null, a boolean, an integer where the number is one (to 2^64
- 1), else a float, a text, an array, a map of text keys in their order.

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
    auto j = encoding::json::parse(R"({"id": 7, "ratio": 0.5, "tags": ["a", "b"], "ok": null})").value();
    encoding::cbor c = encoding::cbor::from_json(j);
    println("{} bytes of CBOR, {} of JSON", c.to_bytes().size(), j.to_string().size());
    println(c.to_string());
}
```

Output:

```text
28 bytes of CBOR, 47 of JSON
{"id": 7, "ratio": 0.5, "tags": ["a", "b"], "ok": null}
```

## See also

- [to_json](to_json.md)
- [sgcl::encoding::cbor](README.md)
