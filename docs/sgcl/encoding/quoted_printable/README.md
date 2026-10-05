[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::quoted_printable

```cpp
#include "sgcl/encoding/quoted_printable.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class quoted_printable;
}
```

`sgcl::encoding::quoted_printable` is the Quoted-Printable of RFC 2045 §6.7: text that is mostly ASCII kept
readable, every other byte as `=XX`, lines of at most 76 characters broken with a `=` at their end. It is the
transfer encoding of a mail's text that is not plain ASCII ([email](../email/README.md) chooses it by itself),
and the Q of the encoded words of a head is a close relative. A codec is a value, as
[base64](../base64/README.md)'s is: the constant [standard](#member-objects) is the text form with a strict
decoding, [binary](binary.md) and [lenient](lenient.md) the other choices. Go has it as streams alone
(`mime/quotedprintable`'s `Reader` and `Writer`); here the text and the bytes come and go whole
([encode](encode.md), [decode](decode.md)) or as a stream ([encoder_to](encoder_to.md),
[decoder_from](decoder_from.md)).

The text form takes a line break of its input (CRLF, or LF alone) as a line break of the output, CRLF; the
binary form encodes CR and LF as `=0D` and `=0A`, as any other byte, for content that is not text. A space or
a tab before a line break or at the end is escaped (`=20`), since a transport may take white space off the
end of a line.

## Rules

- A codec is two flags, plain data that holds no tracked pointer: the constant is a constant, and a codec
  lives anywhere.
- **The encoding.** A byte of 33 to 126 but `=` is itself; `=` and every byte outside the range is `=XX`
  in capitals; a space or a tab is itself unless a line break or the end follows it. A line of the output
  takes at most 76 characters, its `=` of a soft break included, and an escape is never split by one.
- **Strict by default.** A decoding takes `=XX` in either case, a `=` at the end of a line (after the white
  space a transport may have added) as a soft break, and drops the white space at the end of a line. A
  `=` before anything else is `errc::invalid_escape` at the `=`, an escape cut short by the end
  `errc::unexpected_end`. [lenient()](lenient.md) lets the `=` stand for itself, the robust decoder RFC
  2045 recommends and Python's `quopri` is; a decoding never fails then.
- A decoding keeps a line break as it came, CRLF or LF.
- **The streams are handles**, [quoted_printable::encoder](../quoted_printable-encoder/README.md) and
  [quoted_printable::decoder](../quoted_printable-decoder/README.md): one tracked word to a managed object.
- Nothing waits but the streams' reads and writes, which wait as the stream under them does.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `encoder` | [quoted_printable::encoder](../quoted_printable-encoder/README.md): a writer that encodes into another writer |
| `decoder` | [quoted_printable::decoder](../quoted_printable-decoder/README.md): a reader of the bytes another reader's text decodes to |

## Member objects

| Member | Description |
|---|---|
| `standard` | `static const quoted_printable`: the text form, strict decoding |

## Member functions

#### Choices

| Function | Description |
|---|---|
| [binary](binary.md) | the same codec in the binary form: CR and LF encoded |
| [lenient](lenient.md) | the same codec with a lenient decoding |
| [is_binary](is_binary.md) | whether the codec is binary |
| [is_lenient](is_lenient.md) | whether its decoding is lenient |

#### Whole texts

| Function | Description |
|---|---|
| [encode](encode.md) | the text of bytes |
| [decode](decode.md) | the bytes of a text |

#### Streams

| Function | Description |
|---|---|
| [encoder_to](encoder_to.md) | a writer that encodes into another writer |
| [decoder_from](decoder_from.md) | a reader of the bytes another reader's text decodes to |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


     int main() {
         string text = encoding::quoted_printable::standard.encode("Grüße, café = 100%");
         println("{}", text);
         println("{}", string(encoding::quoted_printable::standard.decode(text).value()));
     }
```

Output:

```text
Gr=C3=BC=C3=9Fe, caf=C3=A9 =3D 100%
Grüße, café = 100%
```

## See also

- [email](../email/README.md): the messages that use it
- [base64](../base64/README.md): the other transfer encoding of MIME
- [error](../error/README.md): what a decoding refuses
- RFC 2045 §6.7; `tests/encoding/quoted_printable.cpp`, against Go's `mime/quotedprintable`
