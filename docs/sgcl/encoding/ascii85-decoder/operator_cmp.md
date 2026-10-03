[sgcl](../../README.md) › [encoding](../README.md) › [ascii85](../ascii85/README.md) › [decoder](README.md)

# sgcl::encoding::operator== (sgcl::encoding::ascii85::decoder)

```cpp
friend bool operator==(const decoder& a, const decoder& b) noexcept;
```

Whether two handles share one stream: a copy and the decoder it was copied from are equal, two decoders made by
two calls of [decoder_from](../ascii85/decoder_from.md) are not, even over one reader. Two that hold none are
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
    encoding::ascii85::decoder a = encoding::ascii85::decoder_from(text);
    encoding::ascii85::decoder b = a;
    encoding::ascii85::decoder c = encoding::ascii85::decoder_from(text);
    encoding::ascii85::decoder none;
    println("{} {} {}", a == b, a != c, none == encoding::ascii85::decoder());
}
```

Output:

```text
true true true
```

## See also

- [(constructor)](ascii85-decoder.md): a copy that shares the stream
- [sgcl::encoding::ascii85::decoder](README.md)
