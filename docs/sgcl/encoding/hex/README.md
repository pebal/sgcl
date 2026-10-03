[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::hex

```cpp
#include "sgcl/encoding/hex.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class hex;
}
```

`sgcl::encoding::hex` is hexadecimal, the base16 of [RFC 4648](https://www.rfc-editor.org/rfc/rfc4648) section 8:
a byte as two digits. It is the codec of [base64](../base64/README.md) with four bits a character and nothing to choose —
no alphabet of one's own, no padding, nothing lenient — so every member is static:
`encoding::hex::encode(digest)` reads as Go's `hex.EncodeToString(digest)`.

[dump](dump.md) is the view `hexdump -C` gives, sixteen bytes a line and their characters, for a log, a test,
or what came over a wire; [dumper_to](dumper_to.md) writes it as a stream, a line as soon as its bytes are
there.

## Rules

- [encode](encode.md) writes lower-case digits, [encode_upper](encode_upper.md) upper-case ones;
  [decode](decode.md) takes either case, as the section allows a decoder to.
- `decode` refuses the first character that is not a digit (`invalid_character` at its offset) and an odd length
  (`unexpected_end` at the end); nothing else can be wrong. Go refuses the same texts, its error naming the byte
  rather than its offset.
- `dump` writes Go's `hex.Dump` line for line: the offset in eight hexadecimal digits (more past 4 GB), two
  spaces, sixteen bytes in two columns of eight, and the bytes as characters between bars, a dot for what is not
  printable ASCII; the short line at the end keeps the columns where they are. There is no closing line with the
  total that `hexdump -C` writes.
- The sizes, the caller's buffers and the streams are as [base64's](../base64/README.md#rules): `encode_to` and `decode_to`
  allocate nothing and are `length_error` into a buffer too small, and the streams are handles,
  [hex::encoder](../hex-encoder/README.md), [hex::decoder](../hex-decoder/README.md) and [hex::dumper](../hex-dumper/README.md), each one
  tracked word to a managed object that holds its 8 KB block and the stream under it.
- Nothing waits but the streams' reads and writes, which wait as the stream under them does.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `hex.EncodeToString`, `DecodeString` | `hex::encode`, `hex::decode`; `encode_upper` for upper-case digits; the decoding takes either case |
| `hex.Encode`, `Decode`, `EncodedLen`, `DecodedLen` | `encode_to`, `decode_to`, `encoded_size`, `max_decoded_size` |
| `hex.NewEncoder`, `NewDecoder` | `hex::encoder_to(w)`, `hex::decoder_from(r)` |
| `hex.Dump`, `hex.Dumper` | `hex::dump`, `hex::dumper_to(w)`: the same lines |
| `hex.InvalidByteError`, `hex.ErrLength` | `hex::error` ([error](../error/README.md)): `invalid_character` at the offset of the byte, `unexpected_end` at the end |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `encoder` | [hex::encoder](../hex-encoder/README.md): a writer that encodes into another writer |
| `decoder` | [hex::decoder](../hex-decoder/README.md): a reader of the bytes another reader's digits decode to |
| `dumper` | [hex::dumper](../hex-dumper/README.md): a writer that writes the dump of what it is given to another writer |

## Member functions

#### Encoding and decoding

| Function | Description |
|---|---|
| [encode](encode.md) | the lower-case digits of bytes (static) |
| [encode_upper](encode_upper.md) | the upper-case digits of bytes (static) |
| [decode](decode.md) | the bytes of digits of either case (static) |
| [encoded_size](encoded_size.md) | the digits a number of bytes takes (static) |
| [max_decoded_size](max_decoded_size.md) | the most bytes a number of digits decodes to (static) |
| [encode_to](encode_to.md) | the digits of bytes, into the caller's buffer (static) |
| [decode_to](decode_to.md) | the bytes of digits, into the caller's buffer (static) |

#### Dumps

| Function | Description |
|---|---|
| [dump](dump.md) | the lines of `hexdump -C` (static) |

#### Streams

| Function | Description |
|---|---|
| [encoder_to](encoder_to.md) | a writer that writes the digits of what it is given to another writer (static) |
| [decoder_from](decoder_from.md) | a reader of the bytes another reader's digits decode to (static) |
| [dumper_to](dumper_to.md) | a writer that writes the dump of what it is given to another writer (static) |

## Complexity

Linear in the size of the input: the loops of [base64](../base64/README.md#complexity), a byte into two digits.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> digest = {byte(0xDE), byte(0xAD), byte(0xBE), byte(0xEF)};
    println(encoding::hex::encode(digest));
    println(encoding::hex::encode_upper(digest));
    auto back = encoding::hex::decode("DeadBeef");  // either case
    println("{} bytes", back->size());
    println(encoding::hex::decode("abc").error().message());
    println(encoding::hex::decode("0x12").error().message());

    print(encoding::hex::dump("Hello, World!\n"));
}
```

Output:

```text
deadbeef
DEADBEEF
4 bytes
offset 3: the input ends inside a byte
offset 1: invalid character 'x'
00000000  48 65 6c 6c 6f 2c 20 57  6f 72 6c 64 21 0a        |Hello, World!.|
```

## See also

- [hex::encoder](../hex-encoder/README.md), [hex::decoder](../hex-decoder/README.md), [hex::dumper](../hex-dumper/README.md): the streams
- [base64](../base64/README.md), [base32](../base32/README.md): the same codec with more bits a character
- [error](../error/README.md), [errc](../errc.md): why a text is not hexadecimal, and where
- [sgcl::encoding](../README.md)
