[sgcl](../../README.md) › [encoding](../README.md) › [base64](README.md)

# sgcl::encoding::base64::decode

```cpp
expected<vector<byte>, error> decode(const string& text) const noexcept;
```

The bytes of a text in the codec's alphabet: Go's `DecodeString`. A strict codec refuses a character outside the
alphabet, a line ending included, and bits past the data in the last character; a [lenient](lenient.md) one
skips `'\r'` and `'\n'` and takes the bits. A [padded](padded.md) codec wants its padding, a codec without padding
refuses it.

The offset of the error is where the text stops being the start of valid base64:

| Text | Offset | Why |
|---|---|---|
| `ab*d` | 2 | the character outside the alphabet |
| `Q=` | 1 | the padding where the data cannot end |
| `QQ=x` | 3 | the first character that is not padding inside the padding |
| `QQ==x` | 4 | the first character after the padding |
| `QR==` | 1 | the character with bits past the data (strict) |
| `QQ`, `QQ=` | 2, 3 | the end of a cut text |

Go names the start of the group for a cut text, and the padding for `Q=` and `QQ=x`. The code is
`invalid_character` for a character outside the alphabet, `unexpected_end` for a cut text and `syntax` for the
rest ([errc](../errc.md)).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to decode |

## Return value

The bytes, or the [error](../error/README.md): its code, its offset and a message, `offset 4: invalid character 0x0A`.

## Complexity

Linear in the size of `text`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* text : {"aGk=", "aGk", "QQ=x", "QR==", "QQ="}) {
        auto bytes = encoding::base64::standard.decode(text);
        if (bytes) {
            println("{}: {}", text, string(bytes));
        } else {
            println("{}: {}", text, bytes.error().message());
        }
    }
}
```

Output:

```text
aGk=: hi
aGk: offset 3: the last group is not padded
QQ=x: offset 3: expected padding, found 'x'
QR==: offset 1: bits past the data in the last character
QQ=: offset 3: the padding is cut short
```

## See also

- [encode](encode.md): the text of bytes
- [decode_to](decode_to.md): into the caller's buffer
- [decoder_from](decoder_from.md): as a stream
- [lenient](lenient.md): a decoding that takes line endings
- [sgcl::encoding::base64](README.md)
