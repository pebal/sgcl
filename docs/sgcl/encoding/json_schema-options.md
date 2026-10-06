[sgcl](../README.md) › [encoding](README.md) › [json_schema](json_schema/README.md)

# sgcl::encoding::json_schema::options

```cpp
#include "sgcl/encoding/json_schema.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json_schema {
    public:
        struct options {
            bool format_assertion = false;
            uint32_t max_depth = 512;
        };
    };
}
```

`sgcl::encoding::json_schema::options` is what [compile](json_schema/compile.md) does. A plain struct: set the fields
that differ and pass it.

## Rules

- `options` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.

## Member objects

| Object | Description |
|---|---|
| `format_assertion` | `format` asserts for the formats the [rules](json_schema/README.md#rules) list, rather than being an annotation; `false` |
| `max_depth` | schemas inside one another while validating, each `$ref` counted: past it the value is invalid (a `$ref` cycle that reads nothing ends there); 512 |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json_schema::options o;
    o.format_assertion = true;
    auto strict = encoding::json_schema::compile(encoding::json::parse(R"({"format": "date"})").value(), o).value();
    auto lax = encoding::json_schema::compile(encoding::json::parse(R"({"format": "date"})").value()).value();
    auto value = encoding::json(string("2026-02-30"));
    println("{} {}", strict.valid(value), lax.valid(value));
}
```

Output:

```text
false true
```

## See also

- [compile](json_schema/compile.md)
- [sgcl::encoding::json_schema](json_schema/README.md)
