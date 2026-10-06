[sgcl](../README.md) › [io](README.md)

# sgcl::io::library_options

```cpp
#include "sgcl/io/library.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct library_options {
        bool global = false;
        bool lazy = false;
    };
}
```

`sgcl::io::library_options` is how [open_library](open_library.md) loads a library beyond the one-line form.

## Member objects

| Field | Description |
|---|---|
| `global` | its symbols for the libraries loaded after it (`RTLD_GLOBAL`); `false`: its own (`RTLD_LOCAL`) |
| `lazy` | a function bound at its first call (`RTLD_LAZY`); `false`: every one at the load (`RTLD_NOW`), a missing one an error then |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto z = io::open_library(io::library_file_name("z"), {.lazy = true});
    println("{}", z.has_value());
}
```

Output:

```text
true
```

## See also

- [open_library](open_library.md)
