[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [encoder](../hex-encoder.md)

# sgcl::encoding::operator== (sgcl::encoding::hex::encoder)

```cpp
friend bool operator==(const encoder& a, const encoder& b) noexcept;
```

Whether two handles share one stream: a copy and the encoder it was copied from are equal, two encoders made by
two calls of [encoder_to](../hex/encoder_to.md) are not, even over one writer. Two that hold none are equal.
`!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the encoders compared |

## Return value

`true` when the two hold the same stream, or both none.

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
    io::buffer out;
    encoding::hex::encoder a = encoding::hex::encoder_to(out);
    encoding::hex::encoder b = a;
    encoding::hex::encoder c = encoding::hex::encoder_to(out);
    println("{} {} {}", a == b, a == c, encoding::hex::encoder() == encoding::hex::encoder());
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](hex-encoder.md): a copy that shares the stream
- [sgcl::encoding::hex::encoder](../hex-encoder.md)
