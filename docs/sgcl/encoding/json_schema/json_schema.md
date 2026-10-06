[sgcl](../../README.md) › [encoding](../README.md) › [json_schema](README.md)

# sgcl::encoding::json_schema::json_schema

```cpp
json_schema() noexcept;                      // (1)
explicit json_schema(const json& schema);    // (2)
```

1. The schema `true`: every value valid.
2. A schema written in the program: [compile](compile.md)`(schema).value()`.

The copy and the move are the implicit ones and copy the handle.

## Parameters

| Parameter | Description |
|---|---|
| `schema` | the schema |

## Complexity

(1) Constant; (2) linear in the size of the schema.

## Exceptions

- (1) None.
- (2) `bad_expected_access<encoding::error>` for a value that is no schema.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::json_schema().valid(encoding::json::parse("[1, {}]").value()));
    encoding::json_schema positive(encoding::json::parse(R"({"type": "number", "exclusiveMinimum": 0})").value());
    println("{} {}", positive.valid(encoding::json(3)), positive.valid(encoding::json(-1)));
}
```

Output:

```text
true
true false
```

## See also

- [compile](compile.md)
- [sgcl::encoding::json_schema](README.md)
