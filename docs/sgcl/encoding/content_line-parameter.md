[sgcl](../README.md) › [encoding](README.md) › [content_line](content_line/README.md)

# sgcl::encoding::content_line::parameter

```cpp
#include "sgcl/encoding/content_line.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class content_line {
    public:
        struct parameter {
            string name;
            vector<string> values;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::content_line::parameter` is a parameter of a [content_line](content_line/README.md): its name,
upper-cased, and its values, quotes taken off and RFC 6868's escapes read; it works with `[name, values]`.

## Member objects

| Object | Description |
|---|---|
| `name` | the name |
| `values` | the values, in order |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::content_line line("ATTENDEE", {{"CN", {"Doe, Jane"}}, {"ROLE", {"CHAIR"}}}, "mailto:jane@example.com");
    print(line.to_string());
}
```

Output:

```text
ATTENDEE;CN="Doe, Jane";ROLE=CHAIR:mailto:jane@example.com
```

## See also

- [params](content_line/param.md)
- [sgcl::encoding::content_line](content_line/README.md)
