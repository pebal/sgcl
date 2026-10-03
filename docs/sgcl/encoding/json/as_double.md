[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::as_double

```cpp
optional<double> as_double() const noexcept;         // (1)
double as_double(double fallback) const noexcept;    // (2)
```

Any number as a `double`, rounded to the nearest one: an integer past 2^53 loses its last digits here, as it does
in Go's `float64`. A number kept as its text is rounded once from its decimal; one out of a double's range
(`1e400`, kept with [options](../json-options.md)`::keep_number_text`) gives none, as does anything that is not a
number.

1. The number, or `nullopt`.
2. The number, or `fallback`: `doc["ratio"].as_double(1.0)`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when there is no number |

## Return value

The number, rounded; (1) `nullopt`, (2) `fallback` when the value is not a number or a double cannot hold it.

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
    encoding::json::options keep;
    keep.keep_number_text = true;
    encoding::json doc = encoding::json::parse(R"([0.1, 3, 9007199254740993, 1e400, "1"])", keep);
    for (auto& v : doc.elements()) {
        println("{} -> {}", v.to_string(), v.as_double());
    }
    println(doc[4].as_double(-1));
}
```

Output:

```text
0.1 -> 0.1
3 -> 3
9007199254740993 -> 9007199254740992
1e400 -> nullopt
"1" -> nullopt
-1
```

## See also

- [as_int](as_int.md), [as_uint](as_uint.md): an integer, exactly
- [number_text](number_text.md): the literal of a number kept as text
- [sgcl::encoding::json](README.md)
