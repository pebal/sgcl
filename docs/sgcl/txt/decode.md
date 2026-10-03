[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::decode

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    string decode(const slice<const byte>& bytes, encoding from);                           // (1)
    expected<string, decode_error> decode(const slice<const byte>& bytes, encoding from,    // (2)
                                          strict_t);
    optional<string> decode(const slice<const byte>& bytes, const string& name);            // (3)
}
```

Returns the text the bytes `bytes` stand for in the encoding `from`, as UTF-8.

1. A byte that means nothing in the encoding is one `U+FFFD`, as an invalid byte of UTF-8 is: nothing is refused.
   That is an invalid sequence of UTF-8 (a `U+FFFD` written in it is a character), a lone surrogate or a unit cut
   short in UTF-16 and UTF-32, a byte over 127 in ASCII, a byte a single byte encoding does not define.
2. With the tag [strict](strict_t.md), `decode(bytes, from, txt::strict)`: the text, or the first such byte as a
   [decode_error](decode_error/README.md) — what a program that must not store a changed text asks.

3. The encoding named by `name`, as [encoding_from_name](encoding_from_name.md) reads it (`iso-8859-2`, `Latin2`,
   `cp1250`, out of a header's `charset=`), then as (1); nothing when nobody knows the name. Not an error: a name out
   of data that means nothing is an ordinary answer, as `encoding_from_name` gives it.

- (1–3) A byte order mark is decoded as the character it is, `U+FEFF`: [detect_bom](detect_bom.md) finds one to
  skip.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the bytes |
| `from` | their encoding |
| `name` | the name of their encoding, in any case, with or without its dashes |

## Return value

1. The text.
2. The text, or the error of the first byte that means nothing.
3. The text, or an empty `optional` when the name is none of an encoding.

## Complexity

Linear in the number of bytes. A run of ASCII in a single byte encoding is copied whole.

## Exceptions

`length_error` when the bytes are more than a third of a string's `max_size()`, the bound the text is written into: `sgcl::txt::decode: a text longer than a string can hold`.

## Notes

That byte is nothing in windows-1250; in Latin-1 every byte is a code point. What comes back from [percent::decode](percent/decode.md) is bytes, which this turns into text when they are not UTF-8.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    byte one[] = {byte(0x81)};
    for (auto e : {txt::encoding::windows1250, txt::encoding::latin1}) {
        for (char32_t c : txt::decode(one, e).runes()) {
            println("{}: U+{:04X}", txt::name_of(e), uint32_t(c));
        }
    }
    auto checked = txt::decode(one, txt::encoding::windows1250, txt::strict);
    println("strict: byte {}, {}", checked.error().offset(), checked.error().message());

    byte latin2[] = {byte(0xB1), byte(0x62), byte(0xEA)};
    println("{}", txt::decode(latin2, "ISO-8859-2").value_or("?"));
    println("{}", txt::decode(latin2, "klingon").has_value());
}
```

Output:

```text
windows-1250: U+FFFD
iso-8859-1: U+0081
strict: byte 0, not windows-1250
ąbę
false
```

## See also

- [encode](encode.md): the way back
- [decode_error](decode_error/README.md), [strict_t](strict_t.md)
- [from_utf16](from_utf16.md), [from_utf32](from_utf32.md): units rather than bytes
- [sgcl::txt](README.md)
