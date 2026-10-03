[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex/README.md) › [decoder](README.md)

# sgcl::encoding::operator== (sgcl::encoding::hex::decoder)

```cpp
friend bool operator==(const decoder& a, const decoder& b) noexcept;
```

Whether two handles share one stream: a copy and the decoder it was copied from are equal, two decoders made by
two calls of [decoder_from](../hex/decoder_from.md) are not, even over one reader. Two that hold none are
equal. `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the decoders compared |

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
    io::buffer text;
    encoding::hex::decoder a = encoding::hex::decoder_from(text);
    encoding::hex::decoder b = a;
    encoding::hex::decoder c = encoding::hex::decoder_from(text);
    println("{} {} {}", a == b, a != c, encoding::hex::decoder() == encoding::hex::decoder());
}
```

Output:

```text
true true true
```

## See also

- [(constructor)](hex-decoder.md): a copy that shares the stream
- [sgcl::encoding::hex::decoder](README.md)
