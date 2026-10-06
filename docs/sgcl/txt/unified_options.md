[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::unified_options

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct unified_options {
        size_t context = 3;
        string old_name = string("a");
        string new_name = string("b");
        diff_algorithm algorithm = diff_algorithm::myers;
        bool ignore_whitespace = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::unified_options` is how [unified_diff](unified_diff.md) writes a patch.

## Member objects

| Field | Description |
|---|---|
| `context` | the lines kept around each change, `diff -U`; 3 by default |
| `old_name`, `new_name` | the names on the `---` and `+++` lines; `a` and `b` by default (git writes `a/path` and `b/path`) |
| `algorithm` | the [diff_algorithm](diff_algorithm.md); Myers by default |
| `ignore_whitespace` | lines that differ only in white space count as equal; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    print("{}", txt::unified_diff("a\nb\nc\n", "a\nc\n",
                                  {.context = 0, .old_name = "old", .new_name = "new"}));
}
```

Output:

```text
--- old
+++ new
@@ -2 +1,0 @@
-b
```

## See also

- [unified_diff](unified_diff.md)
- [diff_options](diff_options.md)
- [sgcl::txt](README.md)
