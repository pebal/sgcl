[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::as_int

```cpp
optional<int64_t> as_int() const noexcept;          // (1)
int64_t as_int(int64_t fallback) const noexcept;    // (2)
```

The number as an `int64_t`, exactly or not at all: a number whose value is an integer an `int64_t` holds, however
it is held — `2`, `2.0`, `1e2`, a literal kept as its text whose value is one. `2.5`, 2^63 and anything that is not
a number give none; a string of digits is a string, not a number.

1. The integer, or `nullopt`.
2. The integer, or `fallback`: `doc["count"].as_int(0)`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when there is no integer |

## Return value

The integer; (1) `nullopt`, (2) `fallback` when the value is not a number that an `int64_t` holds exactly.

## Complexity

Constant; linear in the length of the literal for a number kept as its text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(
        R"([2, 2.0, 1e2, 2.5, 9223372036854775808, -9223372036854775808, "2"])");
    for (auto& v : doc.elements()) {
        println("{} -> {}", v.to_string(), v.as_int());
    }
    println(doc[3].as_int(-1));
}
```

Output:

```text
2 -> 2
2 -> 2
100 -> 100
2.5 -> nullopt
9223372036854775808 -> nullopt
-9223372036854775808 -> -9223372036854775808
"2" -> nullopt
-1
```

## See also

- [as_uint](as_uint.md): the number as an `uint64_t`
- [as_double](as_double.md): any number, rounded
- [is_integer](is_integer.md): whether there is an integer
- [sgcl::encoding::json](README.md)
