[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::byte_order_mark

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct byte_order_mark {
        optional<encoding> says;
        size_t size = 0;
    };
}
```

`sgcl::txt::byte_order_mark` is what [detect_bom](detect_bom.md) finds at the front of some bytes: the encoding a
byte order mark announces and how many bytes it takes. A plain struct; converted to `bool`, whether there was one.

## Member objects

| Member | Description |
|---|---|
| `says` | the encoding the mark announces — `utf8`, `utf16le`, `utf16be`, `utf32le` or `utf32be` — or `nullopt` when there is no mark |
| `size` | the bytes the mark takes, to be skipped before decoding: 3, 2 or 4; `0` when there is none |

## Member functions

| Function | Description |
|---|---|
| [operator bool](byte_order_mark/operator_bool.md) | checks whether there was a mark |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    byte page[] = {byte(0xEF), byte(0xBB), byte(0xBF),
                   byte('c'), byte('z'), byte(0xC5), byte(0x82)};
    txt::byte_order_mark bom = txt::detect_bom(page);
    println("{} {} bytes", txt::name_of(*bom.says), bom.size);
    println("{}", txt::decode(slice<const byte>(page).subslice(bom.size), *bom.says));
}
```

Output:

```text
utf-8 3 bytes
czł
```

## See also

- [detect_bom](detect_bom.md)
- [sgcl::txt](README.md)
