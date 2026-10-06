[sgcl](../README.md) › [encoding](README.md) › [cbor](cbor/README.md)

# sgcl::encoding::cbor::style

```cpp
#include "sgcl/encoding/cbor.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class cbor {
    public:
        struct style {
            bool sort_keys = false;
        };

        static const style preferred;
        static const style deterministic;
    };
}
```

`sgcl::encoding::cbor::style` is how [to_bytes](cbor/to_bytes.md) writes a value: `cbor::preferred`, the preferred
serialization of RFC 8949 §4.1, and `cbor::deterministic`, the same with every map's keys sorted by the bytes of their
encodings (§4.2.1), so that equal values write equal bytes — what a signature or a hash over CBOR needs.

## Rules

- `style` is plain data and holds no pointer: it lives anywhere.

## Member objects

| Object | Description |
|---|---|
| `sort_keys` | the keys of every map sorted by their encodings; `false` |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor m = encoding::cbor::map({{"z", 1}, {"a", 2}});
    println(encoding::hex::encode(m.to_bytes(encoding::cbor::preferred)));
    println(encoding::hex::encode(m.to_bytes(encoding::cbor::deterministic)));
}
```

Output:

```text
a2617a01616102
a2616102617a01
```

## See also

- [to_bytes](cbor/to_bytes.md)
- [sgcl::encoding::cbor](cbor/README.md)
