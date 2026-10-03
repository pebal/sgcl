[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [token](../json-token.md)

# sgcl::encoding::json::token::as_int

```cpp
/*(1)*/ optional<int64_t> as_int() const noexcept;
/*(2)*/ int64_t as_int(int64_t fallback) const noexcept;
```

A number token as an `int64_t`, when the value of its literal is an integer that an `int64_t` holds exactly,
whatever the literal's form: `1e2` is 100 and `-3.0` is −3, while `1.5` and `9223372036854775808` (2^63) are none.
Go's v2 `Token.Int()`, which truncates a fraction and saturates past the range where this gives none.

1. The value, or `nullopt` for a number that is not such an integer and for a token of another kind.
2. The value, or `fallback` where (1) gives `nullopt`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what a token without such a value gives |

## Return value

The integer; (1) `nullopt`, (2) `fallback` for a token without one.

## Complexity

Linear in the length of the literal.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::reader r(string(R"([42, 1e2, -3.0, 1.5, 9223372036854775808, "7"])"));
    r.next();
    while (r.more()) {
        auto t = r.next();
        println("{} {}", t->text(), t->as_int(-1));
    }
}
```

Output:

```text
42 42
1e2 100
-3.0 -3
1.5 -1
9223372036854775808 -1
7 -1
```

## See also

- [as_uint](as_uint.md): the same as an `uint64_t`
- [as_double](as_double.md): any number, rounded
- [sgcl::encoding::json::token](../json-token.md)
