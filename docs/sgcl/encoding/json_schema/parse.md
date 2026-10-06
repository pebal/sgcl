[sgcl](../../README.md) › [encoding](../README.md) › [json_schema](README.md)

# sgcl::encoding::json_schema::parse, load

```cpp
static expected<json_schema, error> parse(const string& text) noexcept;    // (1)
static expected<json_schema, error> load(const string& path);              // (2)
```

1. The schema of a JSON text: [json::parse](../json/parse.md), then [compile](compile.md).
2. The schema of a file: [json::load](../json/load.md), then compile.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the schema's text |
| `path` | the schema's file |

## Return value

The schema, or the [error](../error/README.md): the reading's, with its place, or compile's.

## Complexity

Linear in the size of the schema.

## Exceptions

- (1) None.
- (2) What a read of the file throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto schema = encoding::json_schema::parse(R"({"type": "array", "items": {"type": "string"}})");
    println(schema->valid(encoding::json::parse(R"(["a", "b"])").value()));
    println(encoding::json_schema::parse("{\"type\": ").error().message());
    println(encoding::json_schema::load("missing.json").error().message());
}
```

Output:

```text
true
1:10: unexpected end of input, expected a value
input/output error: open missing.json: No such file or directory
```

## See also

- [compile](compile.md)
- [sgcl::encoding::json_schema](README.md)
