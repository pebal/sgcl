[sgcl](../README.md) › [encoding](README.md) › [ini](ini/README.md)

# sgcl::encoding::ini::member

```cpp
#include "sgcl/encoding/ini.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class ini {
    public:
        struct member {
            string key;
            string value;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::ini::member` is an entry of a section of an [ini](ini/README.md): a key and its value, what
[sections](ini/sections.md) gives in each section; it works with `[key, value]`.

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
    auto config = encoding::ini::parse("[server]\nhost = example.com\nport = 8080\n").value();
    for (const encoding::ini::section& s : config.sections()) {
        for (const encoding::ini::member& m : s.members) {
            println("{}.{} = {}", s.name, m.key, m.value);
        }
    }
}
```

Output:

```text
server.host = example.com
server.port = 8080
```

## See also

- [sections](ini/sections.md)
- [sgcl::encoding::ini](ini/README.md)
