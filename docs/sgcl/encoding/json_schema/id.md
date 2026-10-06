[sgcl](../../README.md) › [encoding](../README.md) › [json_schema](README.md)

# sgcl::encoding::json_schema::id

```cpp
string id() const noexcept;
```

The root's `$id`, resolved: the URI the schema's absolute locations start with; `https://schema.invalid/root.json`
for a schema without one.

## Parameters

None.

## Return value

The URI.

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
    println(schema.id());
    println(encoding::json_schema().id());
}
```

Output:

```text
https://example.com/order.json
https://schema.invalid/root.json
```

## See also

- [sgcl::encoding::json_schema](README.md)
