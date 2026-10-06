[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::patch_options

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct patch_options {
        size_t fuzz = 2;
        bool reverse = false;
    };
}
```

`sgcl::txt::patch_options` is how [apply_patch](apply_patch.md) applies a patch.

## Member objects

| Field | Description |
|---|---|
| `fuzz` | the lines of context at each end of a hunk that may go uncompared, `patch -F`; 2 by default, 0 for a patch that must apply as written |
| `reverse` | undo the patch, `patch -R`: its removed lines inserted and its inserted lines removed; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string patch = "@@ -1,5 +1,5 @@\n a\n b\n-c\n+C\n d\n e\n";
    println("{}", txt::apply_patch("a\nB\nc\nd\ne\n", patch, {.fuzz = 0}).has_value());
    print("{}", txt::apply_patch("a\nB\nc\nd\ne\n", patch).value());
}
```

Output:

```text
false
a
B
C
d
e
```

## See also

- [apply_patch](apply_patch.md)
- [sgcl::txt](README.md)
