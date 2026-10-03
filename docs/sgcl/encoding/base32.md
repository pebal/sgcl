[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::base32

```cpp
#include "sgcl/encoding/base32.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class base32;
}
```

`sgcl::encoding::base32` is the Base32 of [RFC 4648](https://www.rfc-editor.org/rfc/rfc4648) sections 6 and 7:
five bytes as eight characters of an alphabet of 32, letters of one case and digits — a text a person reads aloud
or types, a name on a file system that does not care about case, the secret of a one-time password. It is the
same codec as [base64](base64.md) with five bits a character, and has the same members: a value with
[encode](base32/encode.md) and [decode](base32/decode.md) as its methods, the two alphabets of the RFC as
constants, [standard](#member-objects) and `hex`, and an alphabet of one's own as a
[constructor](base32/base32.md). `std` has no Base32.

Where it differs from Go is what a decoding accepts: Go has no strict base32, and reads texts this decoding
refuses as some bytes or as none ([From code written for Go](#from-code-written-for-go)).

## Rules

- The rules of [base64](base64.md#rules) hold here with 8 characters for 5 bytes: a codec is plain data and lives
  anywhere; strict by default and [lenient()](base32/lenient.md) on request; a padded codec wants its padding and
  a codec without padding refuses it; the offset of an error is where the text stops being the start of valid
  base32; the sizes never wrap; `encode_to` and `decode_to` allocate nothing; the streams are handles.
- **The letters are upper case**, as the RFC writes them; a lower-case letter is outside the alphabet. A program
  that takes either case makes an alphabet of its own for the lower case, or upper-cases the text first.
- **A short group without padding ends only where a byte ends**: after 2, 4, 5 or 7 characters. `MFR`, three
  characters, one byte and seven bits more, is refused at its end.
- **What follows the padding is refused**: `MY======M` fails at the `M` after the padding.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `base32.StdEncoding`, `HexEncoding` | `base32::standard`, `base32::hex`; the members of [base64](base64.md) |
| `base32.NewEncoding(alphabet)`, `WithPadding(base32.NoPadding)` | `base32(alphabet, padding)`, `base32(alphabet, nullopt)`, `without_padding()` |
| a decoding that takes bits past the data (Go has no strict base32) | `lenient()`; the default refuses them |
| `MY======M`: a group of fewer than eight characters after a padded one dropped, whatever it holds (`f`) | refused at the `M` |
| `MFR`: a short group that ends inside a byte taken for nothing, with no error | refused at its end |
| `NoPadding` taking the byte `0xFF` for padding (its `NoPadding` is -1, and -1 as a byte is `0xFF`) | `0xFF` is a character outside the alphabet |
| `EncodeToString`, `DecodeString`, `Encode`, `Decode`, `EncodedLen`, `DecodedLen` | `encode`, `decode`, `encode_to`, `decode_to`, `encoded_size`, `max_decoded_size` |
| `NewEncoder`, `NewDecoder` | `encoder_to(w)`, `decoder_from(r)` |

The tests hold both sides to each of these differences by name.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](error.md) |
| `encoder` | [base32::encoder](base32-encoder.md): a writer that encodes into another writer |
| `decoder` | [base32::decoder](base32-decoder.md): a reader of the bytes another reader's text decodes to |

## Member objects

| Constant | Description |
|---|---|
| `standard` | `static`: RFC 4648 section 6, `A`–`Z` and `2`–`7`, padded with `=` |
| `hex` | `static`: section 7, "base32hex", `0`–`9` and `A`–`V`, which keeps the order of the bytes in the order of the text; padded with `=` |

Both are constant expressions.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](base32/base32.md) | a codec of an alphabet of one's own |

#### Variants

| Function | Description |
|---|---|
| [without_padding](base32/without_padding.md) | the same codec without padding |
| [lenient](base32/lenient.md) | the same codec, its decoding lenient |

#### Observers

| Function | Description |
|---|---|
| [padded](base32/padded.md) | whether the codec pads its last group |
| [is_lenient](base32/is_lenient.md) | whether its decoding is lenient |

#### Encoding and decoding

| Function | Description |
|---|---|
| [encode](base32/encode.md) | the text of bytes |
| [decode](base32/decode.md) | the bytes of a text |
| [encoded_size](base32/encoded_size.md) | the characters a number of bytes takes |
| [max_decoded_size](base32/max_decoded_size.md) | the most bytes a number of characters decodes to |
| [encode_to](base32/encode_to.md) | the text of bytes, into the caller's buffer |
| [decode_to](base32/decode_to.md) | the bytes of a text, into the caller's buffer |

#### Streams

| Function | Description |
|---|---|
| [encoder_to](base32/encoder_to.md) | a writer that writes the encoding of what it is given to another writer |
| [decoder_from](base32/decoder_from.md) | a reader of the bytes another reader's text decodes to |

## Complexity

Linear in the size of the input: the loops of [base64](base64.md#complexity), a group of five bytes into a word
and eight characters out of it.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::base32::standard.encode("foobar"));
    println(encoding::base32::hex.encode("foobar"));
    // the secret of a one-time password: base32 without padding
    auto secret = encoding::base32::standard.without_padding().decode("JBSWY3DPEHPK3PXP");
    println("{} bytes", secret->size());
    // strict: the bits past the data must be zero
    auto odd = encoding::base32::standard.decode("MZ======");
    println(odd.error().message());
    println("{} byte", encoding::base32::standard.lenient().decode("MZ======")->size());
}
```

Output:

```text
MZXW6YTBOI======
CPNMUOJ1E8======
10 bytes
offset 1: bits past the data in the last character
1 byte
```

## See also

- [base32::encoder](base32-encoder.md), [base32::decoder](base32-decoder.md): the streams
- [base64](base64.md): the same codec with six bits a character
- [hex](hex.md): four bits a character
- [error](error.md), [errc](errc.md): why a text is not base32, and where
- [sgcl::encoding](README.md)
