[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::diff_edit

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct diff_edit {
        diff_kind kind = diff_kind::equal;
        size_t old_begin = 0, old_end = 0;
        size_t new_begin = 0, new_end = 0;
    };
}
```

`sgcl::txt::diff_edit` is one run of an edit script: units both texts share, units removed from the old one or units
inserted from the new one. Its ranges are bytes of the two texts whatever the unit (lines, words, code points), so
the caller slices the texts with them directly; consecutive edits cover both texts without a gap, an `insert` has an
empty old range and a `remove` an empty new one, and no two edits in a row are of one kind.

## Member objects

| Field | Description |
|---|---|
| `kind` | the [diff_kind](diff_kind.md): `equal`, `insert` or `remove` |
| `old_begin`, `old_end` | the bytes of the old text; where the run stands in it, for an insert |
| `new_begin`, `new_end` | the bytes of the new text; where the run stands in it, for a removal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (const auto &e : txt::diff_words("red car", "blue car")) {
        println("{} old [{}, {}) new [{}, {})", int(e.kind), e.old_begin, e.old_end, e.new_begin,
                e.new_end);
    }
}
```

Output:

```text
2 old [0, 3) new [0, 0)
1 old [3, 3) new [0, 4)
0 old [3, 7) new [4, 8)
```

## See also

- [diff_lines](diff_lines.md)
- [diff_kind](diff_kind.md)
- [sgcl::txt](README.md)
