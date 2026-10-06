[sgcl](../README.md) › [encoding](README.md) › [dotenv](dotenv/README.md)

# sgcl::encoding::dotenv::member

```cpp
#include "sgcl/encoding/dotenv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class dotenv {
    public:
        struct member {
            string key;
            string value;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::dotenv::member` is an entry of a [dotenv](dotenv/README.md): a key and its value, what
[from](dotenv/from.md) takes and [members](dotenv/members.md) gives; it works with `[key, value]`.

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
    encoding::dotenv::member m{"PORT", "8080"};
    print(encoding::dotenv::from({m}).to_string());
}
```

Output:

```text
PORT=8080
```

## See also

- [from](dotenv/from.md), [members](dotenv/members.md)
- [sgcl::encoding::dotenv](dotenv/README.md)
