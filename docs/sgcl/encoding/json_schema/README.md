[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::json_schema

```cpp
#include "sgcl/encoding/json_schema.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json_schema;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::json_schema` is a [JSON Schema](https://json-schema.org/specification) of draft 2020-12, compiled:
its resources, anchors and references indexed, its keywords found and its patterns compiled once, then any number
of [json](../json/README.md) values validated, from any number of threads at once (it never changes). One word shared
by copying. [valid](valid.md) answers yes or no; [validate](validate.md) gives every keyword that failed with where it
failed in the value and in the schema, in the specification's "basic" output. Go's standard library has no
validator.

## Rules

- **Keywords**: every keyword of the Core and Validation vocabularies — `$id` (base URIs resolved as RFC 3986 says),
  `$anchor`, `$dynamicAnchor`, `$ref` and `$dynamicRef` (to pointers, anchors, embedded resources and the resources
  given to [compile](compile.md)), `$defs`; `allOf`, `anyOf`, `oneOf`, `not`, `if`/`then`/`else`,
  `dependentSchemas`, `prefixItems`, `items`, `contains`, `properties`, `patternProperties`,
  `additionalProperties`, `propertyNames`, `unevaluatedItems`, `unevaluatedProperties`; `type`, `enum`, `const`,
  `multipleOf`, the four bounds, `maxLength`/`minLength` (in code points), `pattern`, the counts of items,
  properties and contained items, `uniqueItems`, `required`, `dependentRequired`, `format`. Other keywords are
  annotations and ignored.
- **Values**: an integer is any number of no fraction (`1.0` is one); numbers compare and divide by their decimal
  digits when they fit 128 bits, so `0.3` is a multiple of `0.1`; equality (`enum`, `const`, `uniqueItems`) is
  [json](../json/README.md)'s, numbers by value and objects as sets.
- **Patterns** are [txt::regex](../../txt/regex/README.md)'s, not anchored, over code points: a pattern of a
  backreference or a lookaround is refused at compile, so no schema can make a validation take exponential time.
- **Formats** are annotations, as 2020-12 says; `format_assertion` in [json_schema::options](../json_schema-options.md)
  makes `date-time`, `date`, `time`, `duration`, `email`, `hostname`, `ipv4`, `ipv6`, `uri`, `uri-reference`,
  `uuid`, `json-pointer`, `relative-json-pointer` and `regex` assert; an unknown format passes.
- **What compile refuses**: a keyword's value of the wrong type, a `$ref` no resource has, a pattern the regex does
  not take (`unsupported_value`), a `$schema` of another draft, an `$id` with a fragment.
- **The cost** of a validation is linear in the value for a schema without `anyOf`, `oneOf` and the unevaluated
  keywords inside one another; those make every branch walk, so a schema from outside the program is the program's
  to trust or to bound with `max_depth`.
- **The oracle**: no validator is on the machine; the tests hold cases written from the two specifications keyword by
  keyword, in the official test suite's form.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `violation` | a failed keyword and where: [json_schema::violation](../json_schema-violation.md) |
| `options` | what compile does: [json_schema::options](../json_schema-options.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](json_schema.md) | the schema true, or a schema written in the program |
| [compile](compile.md) | a schema of a json value, with its resources (static) |
| [parse, load](parse.md) | a schema of a text, of a file (static) |
| [valid](valid.md) | whether a value is valid |
| [validate](validate.md) | every keyword a value fails |
| [id](id.md) | the root's URI |

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
    auto order = encoding::json::parse(R"({"id": "PL-17", "items": [{"sku": "A1", "qty": 2}, {"sku": "B2", "qty": 0}]})").value();
    println(schema.valid(order));
    for (const auto& v : schema.validate(order)) {
        println("{}: {}", v.instance_location, v.message);
    }
}
```

Output:

```text
false
/items/1/qty: 0 is less than the minimum 1
```

## See also

- [json](../json/README.md), [json::patch](../json/patch.md)
- [sgcl::encoding](../README.md)
