[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md) › [decoder](../base64-decoder.md)

# sgcl::encoding::base64::decoder::decoder

```cpp
decoder() noexcept;                        // (1)
decoder(const decoder& other) noexcept;    // (2)
```

1. A decoder that holds no stream: `!d` is `true`, and any other operation on it is a contract violation,
   checked by `assert`. A decoder with a stream is made by a codec's [decoder_from](../base64/decoder_from.md).
2. A handle of the stream `other` holds: the two are one decoder, and what one reads the other does not.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the decoder whose stream is shared |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::base64::decoder none;
    println("{}", !none);

    io::buffer text;
    text.write("aGVsbG8=");
    encoding::base64::decoder plain = encoding::base64::standard.decoder_from(text);
    encoding::base64::decoder copy = plain;
    array<byte, 3> first;
    plain.read(first);
    println("{}", copy.read_all_text().value());
}
```

Output:

```text
true
lo
```

## See also

- [decoder_from](../base64/decoder_from.md): a decoder with a stream
- [operator==](operator_cmp.md): whether two handles share one stream
- [sgcl::encoding::base64::decoder](../base64-decoder.md)
