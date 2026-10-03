[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::operator=

```cpp
big_integer& operator=(const big_integer& other) noexcept;    // (1)
big_integer& operator=(big_integer&& other) noexcept;         // (2)
```

Replaces the value.

1. With the value of `other`. The object of limbs is shared, not copied, and marked shared, with one atomic write
   the first time; from then on neither value writes into it. An assignment to itself does nothing.
2. Takes the object of limbs of `other` over, and leaves `other` zero. A value within `int64_t`, which has no
   object, is copied, and `other` keeps it. An assignment to itself does nothing.

A whole number of the language is assigned through the implicit [constructor](big_integer.md) (2), `a = 7`, and
a text through the explicit one, `a = math::big_integer("ff", 16)`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the value copied or taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Notes

A copy never changes with the value it was taken from: `a += b` means `a = a + b`, and whoever else holds the old
value still has it whole. Only a value whose object nobody else has — the result of an operation, not copied
since — is written in place by `+=`, `-=` and `*=` ([operator_arith](operator_arith.md#notes)), and a move hands
that object on. The object this value held before is left to the collector.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer a = math::big_integer(1) << 100;
    math::big_integer b;
    b = a;
    a += 1;
    println("{}", a);
    println("{}", b);

    math::big_integer c;
    c = std::move(a);
    println("{} {}", c, a);
    c = 7;
    println("{}", c);
}
```

Output:

```text
1267650600228229401496703205377
1267650600228229401496703205376
1267650600228229401496703205377 0
7
```

## See also

- [(constructor)](big_integer.md): constructs a value
- [operator+=](operator_arith.md): the compound assignments
- [sgcl::math::big_integer](README.md)
