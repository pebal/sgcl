[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::detect_bom

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    byte_order_mark detect_bom(const slice<const byte>& bytes) noexcept;
}
```

Returns what a byte order mark at the front of `bytes` says, and how many bytes it takes: `EF BB BF` is UTF-8,
`FF FE 00 00` UTF-32 with the low byte first, `FF FE` UTF-16 with the low byte first, `FE FF` UTF-16 with the high
byte first, `00 00 FE FF` UTF-32 with the high byte first. Nothing else in the module looks at one: a caller who
wants it honoured skips those bytes itself, which keeps that decision where it belongs.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the first bytes of a text, or all of them |

## Return value

A [byte_order_mark](byte_order_mark/README.md): the encoding and the size, or no encoding and 0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    byte page[] = {byte(0xFF), byte(0xFE), byte('h'), byte(0), byte('i'), byte(0)};
    auto bom = txt::detect_bom(page);
    println("{}, {} bytes: {}", txt::name_of(*bom.says), bom.size,
            txt::decode(slice<const byte>(page).subslice(bom.size), *bom.says));
}
```

Output:

```text
utf-16le, 2 bytes: hi
```

## See also

- [byte_order_mark](byte_order_mark/README.md)
- [decode](decode.md)
- [sgcl::txt](README.md)
