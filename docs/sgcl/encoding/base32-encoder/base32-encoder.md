[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32.md) › [encoder](../base32-encoder.md)

# sgcl::encoding::base32::encoder::encoder

```cpp
encoder() noexcept;                        // (1)
encoder(const encoder& other) noexcept;    // (2)
```

1. An encoder that holds no stream: `!e` is `true`, and any other operation on it is a contract violation,
   checked by `assert`. An encoder with a stream is made by a codec's [encoder_to](../base32/encoder_to.md).
2. A handle of the stream `other` holds: the two are one encoder, and what one writes the other's `close()`
   ends.

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
    encoding::base32::encoder none;
    println("{}", !none);

    io::buffer out;
    encoding::base32::encoder b32 = encoding::base32::standard.encoder_to(out);
    encoding::base32::encoder copy = b32;
    b32.write("h");
    copy.write("i");
    copy.close();
    println("{} {}", out.text(), b32.is_closed());
}
```

Output:

```text
true
NBUQ==== true
```

## See also

- [encoder_to](../base32/encoder_to.md): an encoder with a stream
- [operator==](operator_cmp.md): whether two handles share one stream
- [sgcl::encoding::base32::encoder](../base32-encoder.md)
