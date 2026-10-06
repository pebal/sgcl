[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::merge_options

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct merge_options {
        string ours_label = string("ours");
        string base_label = string("base");
        string theirs_label = string("theirs");
        bool diff3 = false;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::merge_options` is how [merge3](merge3.md) writes its conflicts.

## Member objects

| Field | Description |
|---|---|
| `ours_label` | after `<<<<<<<`; `ours` by default (git writes a branch or `HEAD`) |
| `base_label` | after `|||||||`, with `diff3`; `base` by default |
| `theirs_label` | after `>>>>>>>`; `theirs` by default |
| `diff3` | conflicts show the base's lines too, `git merge-file --diff3`, and are not narrowed; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto m =
        txt::merge3("x\n", "mine\n", "yours\n", {.ours_label = "HEAD", .theirs_label = "feature"});
    print("{}", m.text);
}
```

Output:

```text
<<<<<<< HEAD
mine
=======
yours
>>>>>>> feature
```

## See also

- [merge3](merge3.md)
- [merge_result](merge_result.md)
- [sgcl::txt](README.md)
