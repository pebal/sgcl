[sgcl](../README.md) › [math](README.md)

# sgcl::math::parse_error

```cpp
#include "sgcl/math/big_integer.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class parse_error;
}
```

`sgcl::math::parse_error` is why a text did not read as a number: the byte of the text where the reading stopped
and a sentence saying why. It is the error of [big_integer::parse](big_integer/parse.md) and of
[rational::parse](rational/parse.md), and what the `bad_expected_access<parse_error>` thrown by the constructor of
either from a wrong literal carries. A text that is not a number is a fact about the text, not a mistake of the
program, so it comes back as a value in an [expected](../core/expected.md), where Go's `SetString` answers
`nil, false` and says neither where nor why.

Only the parses make one: the class has no public constructor.

## Rules

- A plain value of 16 bytes, trivially copyable: it lives anywhere.
- The offset is the byte where the reading stopped: 0 for an empty text, the first byte that is not a digit of the
  base (for a letter past ASCII, the byte its encoding starts at), the end when there was only a sign; for a
  rational also the first digit of a denominator of zero and the first byte after the `e` of an exponent past a
  million.
- The sentences are the library's, each followed by ` at byte ` and the offset: `"empty text"`,
  `"no digits after the sign"`, `"not a digit in base "` and the base, `"a denominator of zero"`,
  `"an exponent past a million"`.

## Member functions

| Function | Description |
|---|---|
| [offset](parse_error/offset.md) | the byte of the text where the reading stopped |
| [message](parse_error/message.md) | the sentence, with the offset |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](parse_error/operator_cmp.md) | compares two errors |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    string input = "12,000";
    auto n = math::big_integer::parse(input);
    if (!n) {
        const math::parse_error& e = n.error();
        println("\"{}\": {} ({})", input, e.message(), e.offset());
    }
}
```

Output:

```text
"12,000": not a digit in base 10 at byte 2 (2)
```

## See also

- [big_integer::parse](big_integer/parse.md), [rational::parse](rational/parse.md): what returns it
- [expected](../core/expected.md): the value or the error
- [The module](README.md)
