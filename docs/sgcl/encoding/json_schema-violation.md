[sgcl](../README.md) › [encoding](README.md) › [json_schema](json_schema/README.md)

# sgcl::encoding::json_schema::violation

```cpp
#include "sgcl/encoding/json_schema.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json_schema {
    public:
        struct violation {
            string instance_location;
            string keyword_location;
            string absolute_location;
            string message;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::json_schema::violation` is a keyword a value failed, as [validate](json_schema/validate.md) gives
it: the specification's "basic" output unit.

## Member objects

| Object | Description |
|---|---|
| `instance_location` | where in the value: a JSON Pointer, `/items/1/qty`; empty for the value itself |
| `keyword_location` | the path through the schema to the keyword, `$ref`s in it: `/properties/items/items/$ref/required` |
| `absolute_location` | the keyword's place: the URI of the schema resource holding it and its pointer there |
| `message` | what failed, in words |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json_schema schema(encoding::json::parse(R"({"properties": {"age": {"minimum": 0}}})").value());
    for (const auto& [where, keyword, absolute, message] : schema.validate(encoding::json::parse(R"({"age": -3})").value())) {
        println("{}\n{}\n{}\n{}", where, keyword, absolute, message);
    }
}
```

Output:

```text
/age
/properties/age/minimum
https://schema.invalid/root.json#/properties/age/minimum
-3 is less than the minimum 0
```

## See also

- [validate](json_schema/validate.md)
- [sgcl::encoding::json_schema](json_schema/README.md)
