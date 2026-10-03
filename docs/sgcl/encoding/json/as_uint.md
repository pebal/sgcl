[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::as_uint

```cpp
/*(1)*/ optional<uint64_t> as_uint() const noexcept;
/*(2)*/ uint64_t as_uint(uint64_t fallback) const noexcept;
```

The number as an `uint64_t`, exactly or not at all: a number whose value is an integer from 0 to 2^64 − 1,
however it is held — `2`, `2.0`, `1e19`, a literal kept as its text whose value is one. A negative number, `2.5`,
2^64 and anything that is not a number give none.

1. The integer, or `nullopt`.
2. The integer, or `fallback`: `doc["size"].as_uint(0)`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when there is no integer |

## Return value

The integer; (1) `nullopt`, (2) `fallback` when the value is not a number that an `uint64_t` holds exactly.

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
        R"([18446744073709551615, 1e19, -1, 2.5, 18446744073709551616])");
    for (auto& v : doc.elements()) {
        println("{} -> {}", v.to_string(), v.as_uint());
    }
    println(doc[2].as_uint(0));
}
```

Output:

```text
18446744073709551615 -> 18446744073709551615
10000000000000000000 -> 10000000000000000000
-1 -> nullopt
2.5 -> nullopt
18446744073709551616 -> nullopt
0
```

## See also

- [as_int](as_int.md): the number as an `int64_t`
- [as_double](as_double.md): any number, rounded
- [is_integer](is_integer.md): whether there is an integer
- [sgcl::encoding::json](../json.md)
