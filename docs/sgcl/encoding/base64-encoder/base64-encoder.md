[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64/README.md) › [encoder](README.md)

# sgcl::encoding::base64::encoder::encoder

```cpp
encoder() noexcept;                        // (1)
encoder(const encoder& other) noexcept;    // (2)
```

1. An encoder that holds no stream: `!e` is `true`, and any other operation on it is a contract violation,
   checked by `assert`. An encoder with a stream is made by a codec's [encoder_to](../base64/encoder_to.md).
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
    encoding::base64::encoder none;
    println("{}", !none);

    io::buffer out;
    encoding::base64::encoder armored = encoding::base64::standard.encoder_to(out);
    encoding::base64::encoder copy = armored;
    armored.write("h");
    copy.write("i");
    copy.close();
    println("{} {}", out.text(), armored.is_closed());
}
```

Output:

```text
true
aGk= true
```

## See also

- [encoder_to](../base64/encoder_to.md): an encoder with a stream
- [operator==](operator_cmp.md): whether two handles share one stream
- [sgcl::encoding::base64::encoder](README.md)
