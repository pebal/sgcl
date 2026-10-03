[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::as_bool

```cpp
optional<bool> as_bool() const noexcept;       // (1)
bool as_bool(bool fallback) const noexcept;    // (2)
```

The boolean of the value.

1. The boolean, or `nullopt` when the value is not one.
2. The boolean, or `fallback` when the value is not one: `doc["tls"].as_bool(false)`.

Nothing is converted: the string `"true"` and the number 1 are not booleans.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when the value is not a boolean |

## Return value

The boolean; (1) `nullopt`, (2) `fallback` when the value is not one.

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
    encoding::json doc = encoding::json::parse(R"({"tls": true, "debug": "true"})");
    println(doc["tls"].as_bool().has_value());
    println(doc["debug"].as_bool().has_value());
    println("{} {}", doc["tls"].as_bool(false), doc["debug"].as_bool(false));
    println(doc["gzip"].as_bool(true));
}
```

Output:

```text
true
false
true false
true
```

## See also

- [is_bool](is_bool.md): whether the value is a boolean
- [as_int](as_int.md), [as_string](as_string.md): the value of another kind
- [sgcl::encoding::json](../json.md)
