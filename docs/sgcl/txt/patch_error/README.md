[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::patch_error

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class patch_error;
}
```

`sgcl::txt::patch_error` is why [apply_patch](../apply_patch.md) did not apply a patch: the hunk and the line of the
patch, and the reason — a header that is not one, a hunk shorter than its header says, a line without a mark, no
hunk at all, or a hunk whose lines are nowhere in the text. A plain value of a few words; it lives anywhere.

## Member functions

| Function | Description |
|---|---|
| [hunk](hunk.md) | the hunk that failed, from 1 |
| [line](line.md) | its line in the patch, from 1 |
| [message](message.md) | why |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto r = txt::apply_patch("one\n", "--- a\n+++ b\n@@ -1 +1 @@\n-two\n+2\n");
    if (!r) {
        println("hunk {} (line {}): {}", r.error().hunk(), r.error().line(), r.error().message());
    }
}
```

Output:

```text
hunk 1 (line 3): a hunk whose lines are not in the text
```

## See also

- [apply_patch](../apply_patch.md)
- [sgcl::txt](../README.md)
