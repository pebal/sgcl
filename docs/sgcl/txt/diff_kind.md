[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::diff_kind

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class diff_kind : uint8_t {
        equal,
        insert,
        remove,
    };
}
```

What a [diff_edit](diff_edit.md) does.

| Value | Description |
|---|---|
| `equal` | units both texts have |
| `insert` | units of the new text only |
| `remove` | units of the old text only |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (const auto &e : txt::diff_chars("ab", "ac")) {
        println("{}", e.kind == txt::diff_kind::equal    ? "equal"
                      : e.kind == txt::diff_kind::insert ? "insert"
                                                         : "remove");
    }
}
```

Output:

```text
equal
remove
insert
```

## See also

- [diff_edit](diff_edit.md)
- [sgcl::txt](README.md)
