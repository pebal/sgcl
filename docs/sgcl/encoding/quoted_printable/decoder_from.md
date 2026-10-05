[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::decoder_from

```cpp
decoder decoder_from(const io::reader& in) const noexcept;
```

A reader of the bytes the text of `in` decodes to, by this codec's decoding: Go's `quotedprintable.NewReader`.
It reads `in` in blocks of 8 KB; the white space at the end of a line waits until the line's end says whether it
is the transport's.

## Parameters

| Parameter | Description |
|---|---|
| `in` | where the text comes from: any reader |

## Return value

The [decoder](../quoted_printable-decoder/README.md), a handle of the new stream.

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
    io::buffer in;
    in.write("caf=C3=A9 au=\r\n lait");
    auto dec = encoding::quoted_printable::standard.decoder_from(in);
    println("{}", io::read_all_text(dec).value());
}
```

Output:

```text
café au lait
```

## See also

- [quoted_printable::decoder](../quoted_printable-decoder/README.md)
- [decode](decode.md): the bytes at once
- [quoted_printable](README.md)
