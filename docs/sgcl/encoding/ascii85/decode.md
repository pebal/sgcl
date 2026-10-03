[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md)

# sgcl::encoding::ascii85::decode

```cpp
static expected<vector<byte>, error> decode(const string& text) noexcept;
```

The bytes of an Ascii85 text, as Go's `ascii85.Decode` reads it: every byte up to the space — white space and the
control characters — is skipped, so a text wrapped at any width reads back; `z` is four zero bytes; a last group
of k characters is padded with `u` and gives k - 1 bytes. The errors ([errc](../errc.md)):

| Error | Where |
|---|---|
| `invalid_character` | a character outside `!` to `u`, `z` and what is skipped: at it |
| `syntax` | `z` inside a group: at it |
| `out_of_range` | a group worth more than 32 bits: at its fifth character, or at the end for a short last group |
| `unexpected_end` | a single character after the last group: at the end |

Go takes a group past 32 bits modulo 2^32 and reads some other bytes. No `<~` and `~>` are taken off: the
caller takes them off.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to decode |

## Return value

The bytes, or the [error](../error.md): its code, its offset and a message.

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
    for (const char* text : {"87cURD]i", "87cUR\nDZ", "87cURD", "87z", "s8W-\"", "<~87cURDZ~>"}) {
        auto bytes = encoding::ascii85::decode(text);
        if (bytes) {
            println("{} bytes", bytes->size());
        } else {
            println("{}", bytes.error().message());
        }
    }
}
```

Output:

```text
6 bytes
5 bytes
offset 6: the input ends inside a byte
offset 2: 'z' inside a group
offset 4: a group past 32 bits
offset 1: invalid character '~'
```

## See also

- [encode](encode.md): the text of bytes
- [decode_to](decode_to.md): into the caller's buffer
- [decoder_from](decoder_from.md): as a stream
- [sgcl::encoding::ascii85](../ascii85.md)
