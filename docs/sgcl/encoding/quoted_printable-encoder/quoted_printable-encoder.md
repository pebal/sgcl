[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [encoder](README.md)

# sgcl::encoding::quoted_printable::encoder::encoder

```cpp
encoder() noexcept;                        // (1)
encoder(const encoder& other) noexcept;    // (2)
```

1. A encoder that holds no stream: `!h` is `true`. One with a stream is made by a codec's
   [encoder_to](../quoted_printable/encoder_to.md).
2. A handle of the stream `other` holds: the two are one encoder.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the encoder whose stream is shared |

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
    encoding::quoted_printable::encoder none;
    println("{}", !none);
    io::buffer b;
    auto made = encoding::quoted_printable::standard.encoder_to(b);
    encoding::quoted_printable::encoder copy = made;
    println("{}", copy == made);
}
```

Output:

```text
true
true
```

## See also

- [encoder](README.md)
