[sgcl](../../README.md) › [encoding](../README.md) › [json_schema](README.md)

# sgcl::encoding::json_schema::validate

```cpp
vector<violation> validate(const json& instance) const;
```

Every keyword the value fails, in the schema's order, each a [violation](../json_schema-violation.md): where in the
value, the path through the schema (`$ref`s in it), the keyword's absolute place and a message. A failed `anyOf` or
`oneOf` of no matching branch gives its branches' violations and its own; a branch that matches leaves none. Empty
exactly when [valid](valid.md) is `true`.

## Parameters

| Parameter | Description |
|---|---|
| `instance` | the value |

## Return value

The violations.

## Complexity

Linear in the value and the violations, for most schemas.

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
    auto order = encoding::json::parse(R"({"id": "X", "items": [{"sku": 1, "qty": 1}, {"qty": 0}]})").value();
    for (const auto& v : schema.validate(order)) {
        println("{} | {} | {}", v.instance_location, v.keyword_location, v.message);
    }
}
```

Output:

```text
/id | /properties/id/pattern | a string the pattern "^[A-Z]{2}-[0-9]+$" does not match
/items/0/sku | /properties/items/items/$ref/properties/sku/type | an integer where the type is "string"
/items/1 | /properties/items/items/$ref/required | a required property is missing: sku
/items/1/qty | /properties/items/items/$ref/properties/qty/minimum | 0 is less than the minimum 1
```

## See also

- [valid](valid.md)
- [json_schema::violation](../json_schema-violation.md)
- [sgcl::encoding::json_schema](README.md)
