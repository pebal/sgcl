[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [decoder](README.md)

# sgcl::encoding::quoted_printable::decoder::decoder

```cpp
decoder() noexcept;                        // (1)
decoder(const decoder& other) noexcept;    // (2)
```

1. A decoder that holds no stream: `!h` is `true`. One with a stream is made by a codec's
   [decoder_from](../quoted_printable/decoder_from.md).
2. A handle of the stream `other` holds: the two are one decoder.

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
    encoding::quoted_printable::decoder none;
    println("{}", !none);
    io::buffer b;
    auto made = encoding::quoted_printable::standard.decoder_from(b);
    encoding::quoted_printable::decoder copy = made;
    println("{}", copy == made);
}
```

Output:

```text
true
true
```

## See also

- [decoder](README.md)
