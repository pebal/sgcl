[sgcl](../README.md) › [encoding](README.md) › [uuid](uuid/README.md)

# sgcl::encoding::uuid::variant_kind

```cpp
#include "sgcl/encoding/uuid.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class uuid {
    public:
        enum class variant_kind : uint8_t {
            ncs,
            rfc9562,
            microsoft,
            future
        };
    };
}
```

`sgcl::encoding::uuid::variant_kind` is the variant field of a [uuid](uuid/README.md) (RFC 9562 §4.1), the top bits
of its ninth byte, as [variant](uuid/variant.md) gives it: which layout the rest of the bytes follows.

| Value | Description |
|---|---|
| `ncs` | `0xxx`: the NCS layout of before RFC 4122; the nil UUID |
| `rfc9562` | `10xx`: the layout of RFC 9562 and RFC 4122, every version it names |
| `microsoft` | `110x`: Microsoft's GUIDs of old |
| `future` | `111x`: reserved; the max UUID |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::uuid::v4().variant() == encoding::uuid::variant_kind::rfc9562);
    println(encoding::uuid::max().variant() == encoding::uuid::variant_kind::future);
}
```

Output:

```text
true
true
```

## See also

- [variant](uuid/variant.md)
- [sgcl::encoding::uuid](uuid/README.md)
