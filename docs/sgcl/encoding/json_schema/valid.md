[sgcl](../../README.md) › [encoding](../README.md) › [json_schema](README.md)

# sgcl::encoding::json_schema::valid

```cpp
bool valid(const json& instance) const noexcept;
```

Whether the value is valid against the schema: the walk ends at the first keyword that fails. Safe from any number
of threads at once.

## Parameters

| Parameter | Description |
|---|---|
| `instance` | the value |

## Return value

`true` when it is valid.

## Complexity

Linear in the value, for most schemas (see [the cost](README.md#rules)).

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto schema = encoding::json_schema::parse(R"({
        "$id": "https://example.com/order.json",
        "type": "object",
        "required": ["id", "items"],
        "properties": {
            "id": {"type": "string", "pattern": "^[A-Z]{2}-[0-9]+$"},
            "items": {"type": "array", "minItems": 1, "items": {"$ref": "#/$defs/item"}}
        },
        "$defs": {
            "item": {"type": "object", "required": ["sku", "qty"],
                     "properties": {"sku": {"type": "string"}, "qty": {"type": "integer", "minimum": 1}}}
        }
    })").value();
    for (const char* text : {R"({"id": "PL-1", "items": [{"sku": "A", "qty": 1}]})", R"({"id": "pl-1", "items": []})"}) {
        println(schema.valid(encoding::json::parse(text).value()));
    }
}
```

Output:

```text
true
false
```

## See also

- [validate](validate.md)
- [sgcl::encoding::json_schema](README.md)
