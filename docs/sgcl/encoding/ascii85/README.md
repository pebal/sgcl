[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::ascii85

```cpp
#include "sgcl/encoding/ascii85.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class ascii85;
}
```

`sgcl::encoding::ascii85` is Ascii85 as the `btoa` tool writes it and Go's `encoding/ascii85` reads it: four bytes
as a number of 32 bits written in five digits of base 85, the characters `!` (0) to `u` (84) — a quarter more text
than the bytes, where base64 takes a third. Four zero bytes are the one character `z`. The last group of 1 to 3
bytes is padded with zeros, encoded, and cut to its first n + 1 characters, which the decoding pads back with `u`.
There is nothing to choose, so every member is static: `encoding::ascii85::encode(data)`.

The text and the bytes come and go whole ([encode](encode.md), [decode](decode.md)), through the
caller's buffer ([encode_to](encode_to.md), [decode_to](decode_to.md)), or as a stream
([encoder_to](encoder_to.md), [decoder_from](decoder_from.md)), as [base64](../base64/README.md)'s do.

## Rules

- No `<~` and `~>` around the text: that is Adobe's variant, a matter of the file that carries it (PostScript,
  PDF), and the caller's to add or take off.
- The decoding skips every byte up to the space — white space and the control characters — as Go's does, so a
  text wrapped at any width reads back. Any other character outside `!` to `u` and `z` is `invalid_character` at
  its offset.
- `z` stands only between groups; inside one it is `syntax` at its offset. A single character after the last
  group is `unexpected_end`.
- **A group worth more than 32 bits is refused**, `out_of_range` at its fifth character (or at the end, for a
  short last group padded with `u`). No encoder writes one; Go takes it modulo 2^32 and reads some other bytes.
  The tests hold both sides to this by name.
- The caller's buffers are as [base64's](../base64/README.md#rules), the sizes bounds rather than counts (a `z` is one
  character for four bytes), and a buffer too small is `length_error`. The sizes never wrap: a bound no `size_t`
  holds is `SIZE_MAX`.
- The streams are handles, [ascii85::encoder](../ascii85-encoder/README.md) and [ascii85::decoder](../ascii85-decoder/README.md), as
  base64's: one tracked word to a managed object that holds its 8 KB block and the stream under it.
- Nothing waits but the streams' reads and writes, which wait as the stream under them does.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `ascii85.Encode`, `Decode` | `ascii85::encode_to`, `decode_to`, into the caller's buffer; `encode` and `decode` into a string and a vector of their own |
| `ascii85.MaxEncodedLen` | `max_encoded_size`; and `max_decoded_size`, which Go has not |
| `ascii85.NewEncoder`, `NewDecoder` | `ascii85::encoder_to(w)`, `ascii85::decoder_from(r)` |
| `ascii85.CorruptInputError` | `ascii85::error` ([error](../error/README.md)): the code, the offset and a message |
| a group past 32 bits taken modulo 2^32 | refused, `out_of_range` |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `encoder` | [ascii85::encoder](../ascii85-encoder/README.md): a writer that encodes into another writer |
| `decoder` | [ascii85::decoder](../ascii85-decoder/README.md): a reader of the bytes another reader's text decodes to |

## Member functions

#### Encoding and decoding

| Function | Description |
|---|---|
| [encode](encode.md) | the text of bytes (static) |
| [decode](decode.md) | the bytes of a text (static) |
| [max_encoded_size](max_encoded_size.md) | the most characters a number of bytes takes (static) |
| [max_decoded_size](max_decoded_size.md) | the most bytes a number of characters decodes to (static) |
| [encode_to](encode_to.md) | the text of bytes, into the caller's buffer (static) |
| [decode_to](decode_to.md) | the bytes of a text, into the caller's buffer (static) |

#### Streams

| Function | Description |
|---|---|
| [encoder_to](encoder_to.md) | a writer that writes the encoding of what it is given to another writer (static) |
| [decoder_from](decoder_from.md) | a reader of the bytes another reader's text decodes to (static) |

## Complexity

Linear in the size of the input.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::ascii85::encode("Hello, World!"));
    vector<byte> zeros(8);
    println(encoding::ascii85::encode(zeros));
    auto back = encoding::ascii85::decode("87cURD_*#4\nDfTZ)+T");  // white space skipped
    println(string(back));
    println(encoding::ascii85::decode("s8W-\"").error().message());
}
```

Output:

```text
87cURD_*#4DfTZ)+T
zz
Hello, World!
offset 4: a group past 32 bits
```

## See also

- [ascii85::encoder](../ascii85-encoder/README.md), [ascii85::decoder](../ascii85-decoder/README.md): the streams
- [base64](../base64/README.md): a third more text, the characters safe everywhere
- [error](../error/README.md), [errc](../errc.md): why a text is not Ascii85, and where
- [sgcl::encoding](../README.md)
