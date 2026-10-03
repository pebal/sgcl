[sgcl](../../README.md) › [math](../README.md) › [parse_error](README.md)

# sgcl::math::parse_error::message

```cpp
string message() const noexcept;
```

Why the text did not read, as a sentence followed by ` at byte ` and the [offset](offset.md):

- `"empty text"`: the text has no bytes;
- `"no digits after the sign"`: a sign with nothing after it; for a [rational](../rational/README.md) also a slash, a
  point or an `e` with no digits where they belong (`"/3"`, `"3/"`, `"."`, `"1e"`), with or without a sign;
- `"not a digit in base "` and the base: a byte that is not a digit of the base (a space, a separator, a prefix
  such as `0x`, a sign in a denominator);
- `"a denominator of zero"`: a rational's `"3/0"`;
- `"an exponent past a million"`: a rational's exponent past a million either way.

The message is what the `bad_expected_access<parse_error>` of a constructor from a wrong literal says.

## Parameters

None.

## Return value

The sentence.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{}", math::big_integer::parse("").error().message());
    println("{}", math::big_integer::parse("-").error().message());
    println("{}", math::big_integer::parse("0x1f", 16).error().message());
    println("{}", math::rational::parse("3/0").error().message());
    println("{}", math::rational::parse("2e1000001").error().message());
}
```

Output:

```text
empty text at byte 0
no digits after the sign at byte 1
not a digit in base 16 at byte 1
a denominator of zero at byte 2
an exponent past a million at byte 2
```

## See also

- [offset](offset.md): the byte alone
- [sgcl::math::parse_error](README.md)
