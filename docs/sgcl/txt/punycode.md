[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::punycode

```cpp
#include "sgcl/txt/idna.h"   // or "sgcl/txt.h"

namespace sgcl::txt::punycode {
}
```

`txt::punycode` is one label of a domain name between the two ways of writing it, Unicode and ASCII, by
[RFC 3492](https://www.rfc-editor.org/rfc/rfc3492): `bücher` is `bcher-kva`. It is the encoding a whole name is built
on by [idna](idna.md), which is what a program that looks names up calls.

## Rules

- A label goes in and comes out **without the `xn--` prefix**, either way: knowing whether a label has one is IDNA's
  business, and punycode itself is only the encoding.
- Nothing comes back, rather than a wrong answer, when the label cannot be written that way at all: for
  [encode](punycode/encode.md), a delta past the 2³² the RFC gives it, which a label of some thousands of characters
  with a supplementary code point in it reaches; for [decode](punycode/decode.md), a digit that is not one, a number
  that stops in the middle, a counter run past its bound, or a code point that no text can hold.
- **The overflow.** Section 6.4 of RFC 3492 says in as many words that an implementation must detect the overflow of
  its counters rather than let them wrap, and that is where the published attacks on punycode have been: a decoder
  whose `i` wraps writes a code point the label did not ask for, at a position it did not ask for. Every addition and
  every multiplication here is bounded before it is made, the arithmetic is done 64 bits wide against a 32 bit bound,
  and the code point that comes out is checked to be one; the RFC's own decoder lets `n` run to 2³² and leaves it to
  the caller to notice. Eight nines in a row are already past the bound.
- All the sample strings of RFC 3492 section 7.1 hold, both ways.

## Member functions

| Function | Description |
|---|---|
| [decode](punycode/decode.md) | a label from punycode to Unicode |
| [encode](punycode/encode.md) | a label from Unicode to punycode |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto ascii = txt::punycode::encode("bücher");
    println("{}", ascii.value());
    println("{}", txt::punycode::decode(*ascii).value());
    println("{}", txt::punycode::decode("99999999").has_value());
}
```

Output:

```text
bcher-kva
bücher
false
```

## See also

- [idna](idna.md): a whole name, with the `xn--` prefix and the checks
- [txt](README.md)
