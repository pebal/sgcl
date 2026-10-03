[sgcl](../../README.md) › [encoding](../README.md) › [hex](README.md)

# sgcl::encoding::hex::decode

```cpp
static expected<vector<byte>, error> decode(const string& text) noexcept;
```

The bytes of hexadecimal digits of either case, two a byte: Go's `hex.DecodeString`. The first character that is
not a digit is `invalid_character` at its offset, and an odd length `unexpected_end` at the end
([errc](../errc.md)); nothing else can be wrong. Go refuses the same texts, its error naming the byte rather than
its offset. No `0x` before the digits, no space between them.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the digits to decode |

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
    for (const char* text : {"DeadBeef", "abc", "0x12", "de ad"}) {
        auto bytes = encoding::hex::decode(text);
        if (bytes) {
            println("{}: {} bytes", text, bytes->size());
        } else {
            println("{}: {}", text, bytes.error().message());
        }
    }
}
```

Output:

```text
DeadBeef: 4 bytes
abc: offset 3: the input ends inside a byte
0x12: offset 1: invalid character 'x'
de ad: offset 2: invalid character ' '
```

## See also

- [encode](encode.md), [encode_upper](encode_upper.md): the digits of bytes
- [decode_to](decode_to.md): into the caller's buffer
- [sgcl::encoding::hex](README.md)
