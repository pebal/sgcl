[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32/README.md) › [encoder](README.md)

# sgcl::encoding::operator== (sgcl::encoding::base32::encoder)

```cpp
friend bool operator==(const encoder& a, const encoder& b) noexcept;
```

Whether two handles share one stream: a copy and the encoder it was copied from are equal, two encoders made by
two calls of [encoder_to](../base32/encoder_to.md) are not, even over one writer. Two that hold none are equal.
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
    encoding::base32::encoder a = encoding::base32::standard.encoder_to(out);
    encoding::base32::encoder b = a;
    encoding::base32::encoder c = encoding::base32::standard.encoder_to(out);
    println("{} {} {}", a == b, a == c, encoding::base32::encoder() == encoding::base32::encoder());
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](base32-encoder.md): a copy that shares the stream
- [sgcl::encoding::base32::encoder](README.md)
