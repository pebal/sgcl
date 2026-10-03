[sgcl](../../README.md) › [encoding](../README.md) › [pem](README.md)

# sgcl::encoding::pem::parse

```cpp
static expected<pem, error> parse(const string& text) noexcept;
```

The first block of a text from outside — a file, a request, a setting — Go's `pem.Decode` without the rest of the
text. The text before the block is skipped, as section 5.2 lets a file explain itself there; a `-----BEGIN ` counts
only at the start of a line. The grammar read is the lax one of section 3: white space anywhere in the base64
(space, tab, the line endings, vertical tab, form feed), lines of any length, CRLF, white space after the dashes.
The base64 itself is strict: bits past the data in the last character are refused, since a key or a certificate has
one encoding, where Go takes them.

A block that is there and malformed is an error, not a block passed over, as Go's `Decode` passes over it and goes
on to the next, which hides the one a program was looking for.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text with a block |

## Return value

The block, or the [error](../error/README.md), with its line and its column in the text:

- `unexpected_end`: no block in the text (`no PEM block`, at its end), a `BEGIN` without its `END`;
- `syntax`: a boundary line without its closing dashes or with text after them, a type that is not a label, an
  `END` of another type than its `BEGIN`, a header line without a name or one that reads as a boundary line,
  bits past the data in the last character of the base64;
- `invalid_character`, `unexpected_end` and `syntax` of the base64, as [base64](../base64/README.md) reports them: a
  character outside the alphabet, the padding.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string lax = "Our key:\r\n-----BEGIN DATA-----  \r\nQU JD\r\n-----END DATA-----\r\n";
    for (const char* text : {lax.c_str(), "-----BEGIN DATA-----\nQUJ=\n-----END DATA-----\n",
                             "-----BEGIN DATA-----\nQU*D\n-----END DATA-----\n",
                             "-----BEGIN DATA-----\nQUJD\n", "no block here\n"}) {
        auto block = encoding::pem::parse(text);
        if (block) {
            println("{}: {} bytes", block->type(), block->bytes().size());
        } else {
            println("{}", block.error().message());
        }
    }
}
```

Output:

```text
DATA: 3 bytes
2:3: bits past the data in the last character
2:3: invalid character '*'
3:1: no END line for BEGIN DATA
2:1: no PEM block
```

## See also

- [parse_all](parse_all.md): every block of a text
- [(constructor)](pem.md): a block of a text the program writes
- [sgcl::encoding::pem](README.md)
