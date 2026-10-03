[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [token](../json-token.md)

# sgcl::encoding::json::token::as_bool

```cpp
optional<bool> as_bool() const noexcept;       // (1)
bool as_bool(bool fallback) const noexcept;    // (2)
```

The value of a boolean token. Only `true` and `false` are booleans: a string `"true"`, a number and null are not.
Go's v2 `Token.Bool()`, which panics where this gives `nullopt`.

1. The value, or `nullopt` for a token of another kind.
2. The value, or `fallback` for a token of another kind.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what a token of another kind gives |

## Return value

The value of the boolean; (1) `nullopt`, (2) `fallback` for a token of another kind.

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
    encoding::json::reader r(string(R"([true, false, "true", null])"));
    r.next();
    while (r.more()) {
        auto t = r.next();
        println("{} {}", t->as_bool().has_value(), t->as_bool(false));
    }
}
```

Output:

```text
true true
true false
false false
false false
```

## See also

- [as_int](as_int.md), [as_uint](as_uint.md), [as_double](as_double.md): a number's value
- [type](type.md): the kind of the token
- [sgcl::encoding::json::token](../json-token.md)
