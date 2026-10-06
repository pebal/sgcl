[sgcl](../README.md) › [encoding](README.md) › [ini](ini/README.md)

# sgcl::encoding::ini::section

```cpp
#include "sgcl/encoding/ini.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class ini {
    public:
        struct section {
            string name;
            slice<const member> members;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::ini::section` is a section of an [ini](ini/README.md): its name (`""` for the keys before the
first header) and its [entries](ini-member.md) in order, what [sections](ini/sections.md) gives; it works with
`[name, members]`.

## Member objects

| Object | Description |
|---|---|
| `name` | the name, as written between `[` and `]` |
| `members` | the entries, in order |

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
