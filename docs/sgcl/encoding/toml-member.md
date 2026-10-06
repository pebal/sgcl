[sgcl](../README.md) › [encoding](README.md) › [toml](toml/README.md)

# sgcl::encoding::toml::member

```cpp
#include "sgcl/encoding/toml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class toml {
    public:
        struct member {
            string key;
            toml value;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::toml::member` is a member of a table: a key and its value, what [table](toml/table.md) takes and
[members](toml/members.md) gives; it works with `[key, value]`.

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
    encoding::toml::member m{"replicas", 3};
    print(encoding::toml::table({m}).to_string());
}
```

Output:

```text
replicas = 3
```

## See also

- [table](toml/table.md), [members](toml/members.md)
- [sgcl::encoding::toml](toml/README.md)
