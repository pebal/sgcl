[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::base64

```cpp
#include "sgcl/encoding/base64.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class base64;
}
```

`sgcl::encoding::base64` is the Base64 of [RFC 4648](https://www.rfc-editor.org/rfc/rfc4648) sections 4 and 5:
three bytes as four characters of an alphabet of 64. A codec is a value — the alphabet, the padding or none,
strict or lenient — with [encode](base64/encode.md) and [decode](base64/decode.md) as its methods. The four
alphabets Go names are constants of the class, [standard](#member-objects), `url`, `raw_standard` and `raw_url`,
and an alphabet of one's own is a [constructor](base64/base64.md); `encoding::base64::standard.encode(b)` reads
as Go's `base64.StdEncoding.EncodeToString(b)`. `std` has no Base64.

The text and the bytes come and go whole ([encode](base64/encode.md), [decode](base64/decode.md)), through the
caller's buffer ([encode_to](base64/encode_to.md), [decode_to](base64/decode_to.md)), or as a stream:
[encoder_to](base64/encoder_to.md) makes a writer that writes the encoding of what it is given to another
writer, [decoder_from](base64/decoder_from.md) a reader of the bytes another reader's text decodes to.

Where it differs from Go is what a decoding accepts: Go's default takes line endings and the bits past the data
in the last character, which let two texts decode to the same bytes; here a decoding is strict unless asked to
be [lenient](base64/lenient.md), and a text it refuses is an [error](error.md) with the offset of the character
where the text stopped being valid.

## Rules

- A codec is plain data, the characters of its alphabet and a table back, and holds no tracked pointer: the
  constants are constants, and a codec lives anywhere, a global and a `std` container included.
- What a codec returns is the library's: a [string](../core/string.md) for text, a
  [vector\<byte\>](../core/vector.md) for bytes. The bytes it takes are a `slice<const byte>` (a vector, an array,
  a string's bytes, a raw buffer), and a text a `const string&` or a literal. `encode_to` and `decode_to` write
  into the caller's buffer and allocate nothing.
- **Strict by default.** A character outside the alphabet is an error, a line ending included (section 3.3),
  and so are bits past the data in the last character (section 3.5): `QR==` is not `QQ==` written another way,
  it is refused, since two texts that decode to the same bytes are what a signature, a key or a token must not
  allow. [lenient()](base64/lenient.md) skips `'\r'` and `'\n'` anywhere and takes the bits — MIME, PEM files of
  old encoders — and is what Go reads by default: a text Go reads, `lenient()` reads.
- **A padded codec wants its padding, a codec without padding refuses it.** `standard` reads `Zg==` and refuses
  `Zg`; `raw_standard` reads `Zg` and refuses `Zg==`.
- **Where an error is.** The offset is where the text stops being the start of valid base64: the character
  outside the alphabet, the padding where the data cannot end (`Q=`: 1), the first character that is not
  padding inside the padding (`QQ=x`: 3), the first character after it (`QQ==x`: 4), the character with bits
  past the data (`QR==`: 1), the end of a cut text (`QQ`, `QQ=`: the length). Go names the start of the group
  for a cut text, and the padding for the other two; the tests hold both sides to these cases by name.
- **Errors are values.** Nothing throws on the input: a decoding returns `expected<…, error>`. What throws is a
  mistake in the program: an alphabet that is not one (`invalid_argument`, an error at compile time in a
  constant), a caller's buffer smaller than the size the codec asked for, an encoding longer than a string
  holds (`length_error`).
- **Sizes never wrap.** [encoded_size](base64/encoded_size.md) of an `n` whose characters no `size_t` can hold
  is `SIZE_MAX`, the size no buffer has.
- **The streams are handles**, [base64::encoder](base64-encoder.md) and [base64::decoder](base64-decoder.md):
  one tracked word to a managed object that holds its block, 8 KB of `array<byte, N>` as io's buffers are, and
  the stream under it.
- Nothing waits but the streams' reads and writes, which wait as the stream under them does.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `base64.StdEncoding`, `URLEncoding`, `RawStdEncoding`, `RawURLEncoding` | `base64::standard`, `url`, `raw_standard`, `raw_url` |
| `base64.NewEncoding(alphabet)`, `WithPadding(base64.NoPadding)` | `base64(alphabet, padding)`, `base64(alphabet, nullopt)`, `without_padding()` |
| `enc.Strict()` | the default; Go's default is `lenient()` |
| `EncodeToString`, `DecodeString` | `encode`, `decode`; `encode` takes bytes or the bytes of a text |
| `Encode`, `Decode`, `EncodedLen`, `DecodedLen` | `encode_to`, `decode_to`, `encoded_size`, `max_decoded_size`; a buffer too small is `length_error`, a size past `size_t` is `SIZE_MAX` |
| `NewEncoder`, `NewDecoder` | `encoder_to(w)`, `decoder_from(r)`: an `io::writer` and an `io::closer`, an `io::reader`; `close()` leaves the writer under it open, as Go's does; `co_await` `async_write`, `async_read`, `async_close` in a task |
| `CorruptInputError` | `base64::error` ([error](error.md)): the code, the offset and a message |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](error.md) |
| `encoder` | [base64::encoder](base64-encoder.md): a writer that encodes into another writer |
| `decoder` | [base64::decoder](base64-decoder.md): a reader of the bytes another reader's text decodes to |

## Member objects

| Constant | Description |
|---|---|
| `standard` | `static`: RFC 4648 section 4, `A`–`Z`, `a`–`z`, `0`–`9`, `+` and `/`, padded with `=` |
| `url` | `static`: section 5, the alphabet safe in a URL and a file name, `-` and `_` for `+` and `/`, padded with `=` |
| `raw_standard` | `static`: section 4 without padding |
| `raw_url` | `static`: section 5 without padding, a JWT's |

The four are constant expressions: a codec made from one in a `constexpr` is checked at compile time.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](base64/base64.md) | a codec of an alphabet of one's own |

#### Variants

| Function | Description |
|---|---|
| [without_padding](base64/without_padding.md) | the same codec without padding |
| [lenient](base64/lenient.md) | the same codec, its decoding lenient |

#### Observers

| Function | Description |
|---|---|
| [padded](base64/padded.md) | whether the codec pads its last group |
| [is_lenient](base64/is_lenient.md) | whether its decoding is lenient |

#### Encoding and decoding

| Function | Description |
|---|---|
| [encode](base64/encode.md) | the text of bytes |
| [decode](base64/decode.md) | the bytes of a text |
| [encoded_size](base64/encoded_size.md) | the characters a number of bytes takes |
| [max_decoded_size](base64/max_decoded_size.md) | the most bytes a number of characters decodes to |
| [encode_to](base64/encode_to.md) | the text of bytes, into the caller's buffer |
| [decode_to](base64/decode_to.md) | the bytes of a text, into the caller's buffer |

#### Streams

| Function | Description |
|---|---|
| [encoder_to](base64/encoder_to.md) | a writer that writes the encoding of what it is given to another writer |
| [decoder_from](base64/decoder_from.md) | a reader of the bytes another reader's text decodes to |

## Complexity

Linear in the size of the input. Encoding reads a group of three bytes into a word and writes four characters
from a table of 64; decoding reads four characters through a table of 256 into a word and writes three bytes,
and leaves the loop only when a group holds a byte outside the alphabet — a line ending, the padding or an
error — which the rest of the decoder then takes a character at a time. There is no branch in either loop but
that one, and no intrinsic: the loops are scalar, as Go's are, and the compiler is left to them.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // Basic authentication: the bytes of a text
    string header = "Basic " + encoding::base64::standard.encode("ala:sekret");
    println(header);

    // A JWT's segment: the URL alphabet without padding
    auto claims = encoding::base64::raw_url.decode("eyJzdWIiOiI0MiJ9");
    if (claims) {
        println(string(claims));
    }

    // Strict by default: a line ending is not base64
    auto wrapped = encoding::base64::standard.decode("YWxh\nOnNla3JldA==");
    println(wrapped.error().message());
    // MIME wraps its lines: lenient() skips them
    auto mime = encoding::base64::standard.lenient().decode("YWxh\r\nOnNla3JldA==");
    println("{} bytes", mime->size());
}
```

Output:

```text
Basic YWxhOnNla3JldA==
{"sub":"42"}
offset 4: invalid character 0x0A
10 bytes
```

## See also

- [base64::encoder](base64-encoder.md), [base64::decoder](base64-decoder.md): the streams
- [base32](base32.md): the same codec with five bits a character
- [pem](pem.md): base64 between two lines
- [error](error.md), [errc](errc.md): why a text is not base64, and where
- [io streams](../io/README.md)
- [sgcl::encoding](README.md)
