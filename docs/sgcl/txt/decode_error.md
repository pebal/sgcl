[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::decode_error

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class decode_error;
}
```

`sgcl::txt::decode_error` is why bytes are not text in an encoding, as the strict [decode](decode.md) says it: the
first byte that means nothing in it, and the encoding. A plain value of a few bytes; it lives anywhere.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](decode_error/decode_error.md) | constructs the error of a byte in an encoding |

#### Observers

| Function | Description |
|---|---|
| [offset](decode_error/offset.md) | the byte that means nothing, counted from the start |
| [from](decode_error/from.md) | the encoding |
| [message](decode_error/message.md) | the error as a sentence: `not utf-8` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    byte bytes[] = {byte('a'), byte(0x80)};
    auto text = txt::decode(bytes, txt::encoding::ascii, txt::strict);
    if (!text) {
        const txt::decode_error& e = text.error();
        println("{} at byte {} ({})", e.message(), e.offset(), txt::name_of(e.from()));
    }
}
```

Output:

```text
not us-ascii at byte 1 (us-ascii)
```

## See also

- [decode](decode.md), [strict_t](strict_t.md)
- [sgcl::txt](README.md)
