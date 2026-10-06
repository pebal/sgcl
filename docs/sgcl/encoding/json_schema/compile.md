[sgcl](../../README.md) › [encoding](../README.md) › [json_schema](README.md)

# sgcl::encoding::json_schema::compile

```cpp
static expected<json_schema, error> compile(const json& schema) noexcept;                                                     // (1)
static expected<json_schema, error> compile(const json& schema, const options& o) noexcept;                                   // (2)
static expected<json_schema, error> compile(const json& schema, const vector<json>& resources) noexcept;                      // (3)
static expected<json_schema, error> compile(const json& schema, const vector<json>& resources, const options& o) noexcept;    // (4)
```

The schema of a json value, by the [rules](README.md#rules): an object or a boolean. A schema without `$id` has the
base URI `https://schema.invalid/root.json`.

1. With the default [options](../json_schema-options.md).
2. With the options given.
3. With the other documents its `$ref`s reach, each by its `$id` (a validator fetches nothing).
4. The same with the options given.

## Parameters

| Parameter | Description |
|---|---|
| `schema` | the schema |
| `resources` | documents of the schema's references, each with its `$id` |
| `o` | what is done |

## Return value

The schema, or the [error](../error/README.md), without a place, naming where in the schema: `type_mismatch` for a
keyword's value of the wrong type; `missing_field` for a `$ref` no resource has, or a resource without `$id`;
`unsupported_value` for a pattern the regex does not take or a `$schema` of another draft; `syntax` for an `$id`
with a fragment or an anchor that is no plain name.

## Complexity

Linear in the size of the schema and the resources.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<encoding::json> resources;
    resources.push_back(encoding::json::parse(R"({"$id": "https://example.com/address.json", "required": ["city"]})").value());
    auto schema = encoding::json_schema::compile(
        encoding::json::parse(R"({"$id": "https://example.com/person.json", "properties": {"home": {"$ref": "address.json"}}})").value(), resources);
    println(schema->valid(encoding::json::parse(R"({"home": {"street": "Long"}})").value()));
    for (const char* bad : {R"({"type": "text"})", R"({"$ref": "#/$defs/none"})", R"({"pattern": "(a)\\1"})"}) {
        println(encoding::json_schema::compile(encoding::json::parse(bad).value()).error().message());
    }
}
```

Output:

```text
false
type that is none of the seven, or no array of them (at /type)
a reference no resource has: https://schema.invalid/root.json#/$defs/none (at /$ref)
a pattern txt::regex does not take: sgcl::txt::regex: a backreference: this engine matches in time linear in the length of the text, carrying every alternative at once, and nothing in it can be asked to repeat what another part matched (at byte 3 of the pattern) (at /pattern)
```

## See also

- [parse](parse.md)
- [json_schema::options](../json_schema-options.md)
- [sgcl::encoding::json_schema](README.md)
