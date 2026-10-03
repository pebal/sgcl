[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85.md) › [encoder](../ascii85-encoder.md)

# sgcl::encoding::operator== (sgcl::encoding::ascii85::encoder)

```cpp
friend bool operator==(const encoder& a, const encoder& b) noexcept;
```

Whether two handles share one stream: a copy and the encoder it was copied from are equal, two encoders made by
two calls of [encoder_to](../ascii85/encoder_to.md) are not, even over one writer. Two that hold none are equal.
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
    encoding::ascii85::encoder a = encoding::ascii85::encoder_to(out);
    encoding::ascii85::encoder b = a;
    encoding::ascii85::encoder c = encoding::ascii85::encoder_to(out);
    encoding::ascii85::encoder none;
    println("{} {} {}", a == b, a == c, none == encoding::ascii85::encoder());
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](ascii85-encoder.md): a copy that shares the stream
- [sgcl::encoding::ascii85::encoder](../ascii85-encoder.md)
