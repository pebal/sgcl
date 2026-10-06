[sgcl](../README.md) › [encoding](README.md) › [cbor](cbor/README.md)

# sgcl::encoding::cbor::member

```cpp
#include "sgcl/encoding/cbor.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class cbor {
    public:
        struct member {
            cbor key;
            cbor value;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::cbor::member` is a member of a map: a key of any kind and its value, what [map](cbor/map.md) takes
and [members](cbor/members.md) gives; it works with `[key, value]`.

## Member objects

| Object | Description |
|---|---|
| `key` | the key |
| `value` | its value |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor::member m{"answer", 42};
    println("{}: {}", m.key.to_string(), m.value.to_string());
}
```

Output:

```text
"answer": 42
```

## See also

- [map](cbor/map.md), [members](cbor/members.md)
- [sgcl::encoding::cbor](cbor/README.md)
