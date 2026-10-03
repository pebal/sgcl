[sgcl](../../README.md) › [encoding](../README.md) › [base32](README.md)

# sgcl::encoding::base32::decode

```cpp
expected<vector<byte>, error> decode(const string& text) const noexcept;
```

The bytes of a text in the codec's alphabet: Go's `DecodeString`. A strict codec refuses a character outside the
alphabet, a lower-case letter and a line ending included, and bits past the data in the last character; a
[lenient](lenient.md) one skips `'\r'` and `'\n'` and takes the bits. A [padded](padded.md) codec wants its
padding, a codec without padding refuses it. A short group ends only where a byte ends, after 2, 4, 5 or 7
characters, and nothing follows the padding.

The offset of the error is where the text stops being the start of valid base32, as in
[base64's decoding](../base64/decode.md): the character outside the alphabet, the padding where the data cannot
end, the first character that is not padding inside the padding, the first one after it, the character with bits
past the data, the end of a cut text. The code is `invalid_character` for a character outside the alphabet,
`unexpected_end` for a cut text and `syntax` for the rest ([errc](../errc.md)). Go reads some of these texts as
bytes ([From code written for Go](README.md#from-code-written-for-go)).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to decode |

## Return value

The bytes, or the [error](../error/README.md): its code, its offset and a message.

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
    auto raw = encoding::base32::standard.without_padding();
    for (const char* text : {"MZXW6", "MZXW6YQ", "MFR", "mzxw6", "MZ"}) {
        auto bytes = raw.decode(text);
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
MZXW6: foo
MZXW6YQ: foob
MFR: offset 3: the input ends inside a byte
mzxw6: offset 0: invalid character 'm'
MZ: offset 1: bits past the data in the last character
```

## See also

- [encode](encode.md): the text of bytes
- [decode_to](decode_to.md): into the caller's buffer
- [decoder_from](decoder_from.md): as a stream
- [sgcl::encoding::base32](README.md)
