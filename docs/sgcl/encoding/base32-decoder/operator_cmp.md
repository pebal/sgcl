[sgcl](../../README.md) › [encoding](../README.md) › [base32](../base32/README.md) › [decoder](README.md)

# sgcl::encoding::operator== (sgcl::encoding::base32::decoder)

```cpp
friend bool operator==(const decoder& a, const decoder& b) noexcept;
```

Whether two handles share one stream: a copy and the decoder it was copied from are equal, two decoders made by
two calls of [decoder_from](../base32/decoder_from.md) are not, even over one reader. Two that hold none are
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
    encoding::base32::decoder a = encoding::base32::standard.decoder_from(text);
    encoding::base32::decoder b = a;
    encoding::base32::decoder c = encoding::base32::standard.decoder_from(text);
    println("{} {} {}", a == b, a != c, encoding::base32::decoder() == encoding::base32::decoder());
}
```

Output:

```text
true true true
```

## See also

- [(constructor)](base32-decoder.md): a copy that shares the stream
- [sgcl::encoding::base32::decoder](README.md)
