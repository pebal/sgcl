[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md) › [encoder](../ascii85-encoder.md)

# sgcl::encoding::ascii85::encoder::encoder

```cpp
/*(1)*/ encoder() noexcept;
/*(2)*/ encoder(const encoder& other) noexcept;
```

1. An encoder that holds no stream: `!e` is `true`, and any other operation on it is a contract violation,
   checked by `assert`. An encoder with a stream is made by [ascii85::encoder_to](../ascii85/encoder_to.md).
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
    encoding::ascii85::encoder none;
    println("{}", !none);

    io::buffer out;
    encoding::ascii85::encoder a85 = encoding::ascii85::encoder_to(out);
    encoding::ascii85::encoder copy = a85;
    a85.write("h");
    copy.write("i");
    copy.close();
    println("{} {}", out.text(), a85.is_closed());
}
```

Output:

```text
true
BP@ true
```

## See also

- [encoder_to](../ascii85/encoder_to.md): an encoder with a stream
- [operator==](operator_cmp.md): whether two handles share one stream
- [sgcl::encoding::ascii85::encoder](../ascii85-encoder.md)
