[sgcl](../README.md) › [encoding](README.md) › [cbor](cbor/README.md)

# sgcl::encoding::cbor::kind

```cpp
#include "sgcl/encoding/cbor.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class cbor {
    public:
        enum class kind : uint8_t {
            null,
            undefined,
            boolean,
            integer,
            floating,
            bytes,
            text,
            array,
            map,
            tag,
            simple,
            extension
        };
    };
}
```

`sgcl::encoding::cbor::kind` is the kind of a [cbor](cbor/README.md) value, as [type](cbor/type.md) gives it: CBOR's
data model (RFC 8949 §2), and MessagePack's extension beside it.

| Value | Description |
|---|---|
| `null` | null, `f6`; what a lookup that found nothing gives |
| `undefined` | undefined, `f7` |
| `boolean` | `true` or `false` |
| `integer` | -2^64 to 2^64 - 1 (major types 0 and 1) |
| `floating` | a float of half, single or double precision, held as a double |
| `bytes` | a byte string |
| `text` | a text string, UTF-8 |
| `array` | an array |
| `map` | a map of keys of any kind |
| `tag` | a tag: a number and the value it tags |
| `simple` | a simple value other than false, true, null and undefined |
| `extension` | a MessagePack extension: a type and bytes |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor c = encoding::cbor::tagged(1, 5);
    println(c.type() == encoding::cbor::kind::tag);
    println(c.content().type() == encoding::cbor::kind::integer);
}
```

Output:

```text
true
true
```

## See also

- [type](cbor/type.md)
- [sgcl::encoding::cbor](cbor/README.md)
