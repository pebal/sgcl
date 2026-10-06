[sgcl](../README.md) › [io](README.md)

# sgcl::io::glob_options

```cpp
#include "sgcl/io/glob.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct glob_options {
        bool hidden = false;
    };
}
```

`sgcl::io::glob_options` is how a glob pattern matches beyond its text: [glob](glob.md) and
[glob_pattern](glob_pattern/README.md) take it.

## Member objects

| Field | Description |
|---|---|
| `hidden` | wildcards (`*`, `?`, `[...]`, `**`) match names beginning with `.` too, and `**` enters hidden directories: Python's `include_hidden`; `false` by default, when only a component that begins with `.` itself matches such a name |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::glob_pattern visible("**/*.conf");
    io::glob_pattern all("**/*.conf", {.hidden = true});
    println("{} {}", visible.match("etc/.git/x.conf"), all.match("etc/.git/x.conf"));
}
```

Output:

```text
false true
```

## See also

- [glob](glob.md), [glob_pattern](glob_pattern/README.md)
