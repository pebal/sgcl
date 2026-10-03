[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [token](../json-token.md)

# sgcl::encoding::json::token::as_uint

```cpp
/*(1)*/ optional<uint64_t> as_uint() const noexcept;
/*(2)*/ uint64_t as_uint(uint64_t fallback) const noexcept;
```

A number token as an `uint64_t`, when the value of its literal is an integer that an `uint64_t` holds exactly,
whatever the literal's form: `18446744073709551615` (2^64 − 1) and `1e19` are, `-1` and `0.5` are not. Go's v2
`Token.Uint()`, which truncates a fraction and saturates past the range where this gives none.

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
    encoding::json::reader r(string(R"([18446744073709551615, 1e19, -1, 0.5])"));
    r.next();
    while (r.more()) {
        auto t = r.next();
        println("{} {}", t->text(), t->as_uint().has_value() ? string("yes") : string("no"));
    }
    println("{}", r.next()->as_uint(7));
}
```

Output:

```text
18446744073709551615 yes
1e19 yes
-1 no
0.5 no
7
```

## See also

- [as_int](as_int.md): the same as an `int64_t`
- [as_double](as_double.md): any number, rounded
- [sgcl::encoding::json::token](../json-token.md)
