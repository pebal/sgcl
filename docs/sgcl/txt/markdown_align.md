[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::markdown_align

```cpp
#include "sgcl/txt/markdown.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class markdown_align : uint8_t {
        none,
        left,
        center,
        right,
    };
}
```

How a column of a table is aligned, as its delimiter row says: `---`, `:--`, `:-:`, `--:`.

| Value | Description |
|---|---|
| `none` | no colon |
| `left` | a colon on the left |
| `center` | colons on both sides |
| `right` | a colon on the right |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto row = txt::markdown_document::parse("| a | b | c |\n|:-|:-:|-:|\n").root()[0][0];
    println("{} {} {}", int(row[0].align()), int(row[1].align()), int(row[2].align()));
}
```

Output:

```text
1 2 3
```

## See also

- [markdown_node::align](markdown_node/align.md)
- [sgcl::txt](README.md)
