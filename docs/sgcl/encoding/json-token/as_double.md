[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [token](../json-token.md)

# sgcl::encoding::json::token::as_double

```cpp
/*(1)*/ optional<double> as_double() const noexcept;
/*(2)*/ double as_double(double fallback) const noexcept;
```

A number token as the nearest `double`, rounded once from the decimal literal: `0.1` is the `double` nearest to
one tenth, an integer past 2^53 the `double` nearest to it. A literal past the range of a `double` (`1e400`)
gives none; one too small for it (`1e-400`) is 0. Go's v2 `Token.Float()`, which saturates past the range where
this gives none.

1. The value, or `nullopt` past the range and for a token of another kind.
2. The value, or `fallback` where (1) gives `nullopt`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what a token without such a value gives |

## Return value

The nearest `double`; (1) `nullopt`, (2) `fallback` past the range and for a token of another kind.

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
    encoding::json::reader r(string(R"([0.1, 12345678901234567891, 1e400, 1e-400, -0, true])"));
    r.next();
    while (r.more()) {
        auto t = r.next();
        println("{} {}", t->text(), t->as_double(-1));
    }
}
```

Output:

```text
0.1 0.1
12345678901234567891 12345678901234567168
1e400 -1
1e-400 0
-0 -0
true -1
```

## See also

- [as_int](as_int.md), [as_uint](as_uint.md): an integer, exactly
- [sgcl::encoding::json::token](../json-token.md)
