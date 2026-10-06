[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::diff_options

```cpp
#include "sgcl/txt/diff.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct diff_options {
        diff_algorithm algorithm = diff_algorithm::myers;
        bool ignore_whitespace = false;
    };
}
```

`sgcl::txt::diff_options` is how [diff_lines](diff_lines.md), [diff_words](diff_words.md) and
[diff_chars](diff_chars.md) compare.

## Member objects

| Field | Description |
|---|---|
| `algorithm` | the [diff_algorithm](diff_algorithm.md); Myers by default |
| `ignore_whitespace` | units compared without their white space (`diff -w`): lines that differ only in spaces, tabs and line ends are equal, and so are runs of white space between words; `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto e = txt::diff_lines("int x;\n", "int  x ;\n", {.ignore_whitespace = true});
    println("{} {}", e.size(), e[0].kind == txt::diff_kind::equal);
}
```

Output:

```text
1 true
```

## See also

- [diff_lines](diff_lines.md)
- [unified_options](unified_options.md)
- [sgcl::txt](README.md)
