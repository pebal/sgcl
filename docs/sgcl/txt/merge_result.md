[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::merge_result

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct merge_result {
        string text;
        size_t conflicts = 0;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::merge_result` is what [merge3](merge3.md) returns.

## Member objects

| Field | Description |
|---|---|
| `text` | the merged text, its conflicts between markers |
| `conflicts` | the number of conflicts; 0 for a clean merge |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto m = txt::merge3("a\nb\n", "A\nb\n", "a\nB\n");
    print("{}\n{}", m.conflicts, m.text);
}
```

Output:

```text
1
<<<<<<< ours
A
b
=======
a
B
>>>>>>> theirs
```

## See also

- [merge3](merge3.md)
- [merge_options](merge_options.md)
- [sgcl::txt](README.md)
