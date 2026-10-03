[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [encoder](../hex-encoder.md)

# sgcl::encoding::hex::encoder::encoder

```cpp
encoder() noexcept;                        // (1)
encoder(const encoder& other) noexcept;    // (2)
```

1. An encoder that holds no stream: `!e` is `true`, and any other operation on it is a contract violation,
   checked by `assert`. An encoder with a stream is made by [hex::encoder_to](../hex/encoder_to.md).
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
    encoding::hex::encoder none;
    println("{}", !none);

    io::buffer out;
    encoding::hex::encoder digits = encoding::hex::encoder_to(out);
    encoding::hex::encoder copy = digits;
    digits.write("h");
    copy.write("i");
    copy.close();
    println("{} {}", out.text(), digits.is_closed());
}
```

Output:

```text
true
6869 true
```

## See also

- [encoder_to](../hex/encoder_to.md): an encoder with a stream
- [operator==](operator_cmp.md): whether two handles share one stream
- [sgcl::encoding::hex::encoder](../hex-encoder.md)
