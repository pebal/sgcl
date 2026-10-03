[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::operator== (sgcl::encoding::json)

```cpp
friend bool operator==(const json& a, const json& b) noexcept;
```

Whether two values are equal by value, deep; `!=` is made from it by the compiler. Two values of different
[kinds](../json-kind.md) are never equal, and:

- numbers by value: two integers exactly; an integer and a double as doubles (`1 == 1.0`, and an integer past
  2^53 equals the double it rounds to — what a double is written as, read back as an integer, equals it); a
  number kept as its text and another by their digits (`0.10` and `0.1`), and with an integer or a double by its
  value;
- strings by their characters, booleans by their value, and null equals null;
- arrays element by element, in order;
- objects as sets of members, in any order, as JSON means them: the same keys, with equal values.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values compared |

## Return value

`true` when the two values are equal.

## Complexity

Linear in the size of the values; the members of an object are found by key, linear up to 16 members and constant
on average past them.

## Exceptions

None.

## Notes

The comparison walks the two values with a stack of its own: a value deeper than any stack of calls is compared
as well. Equal values have the same [hash](hash.md).

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json one = 1;
    println("{} {}", one == encoding::json(1.0), one == encoding::json("1"));

    encoding::json a = encoding::json::parse(R"({"a": 1, "b": [true, null]})");
    encoding::json b = encoding::json::parse(R"({"b": [true, null], "a": 1})");
    println(a == b);
    println(encoding::json::array({1, 2}) != encoding::json::array({2, 1}));

    encoding::json big = int64_t(9007199254740993);
    println(big == encoding::json(9007199254740992.0));
}
```

Output:

```text
true false
true
true
true
```

## See also

- [hash](hash.md): alike for equal values
- [type](type.md): the kind of a value
- [sgcl::encoding::json](README.md)
