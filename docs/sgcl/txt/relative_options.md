[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::relative_options

```cpp
#include "sgcl/txt/relative_time.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct relative_options {
        txt::width width = txt::width::wide;
        bool numeric = false;
    };
}
```

`sgcl::txt::relative_options` is how [format_relative](format_relative.md) writes a time: its width and whether a
word the language has for -1, 0 or 1 of a unit ("yesterday", "now") stands for the number.

## Member objects

| Field | Description |
|---|---|
| `width` | a [width](width.md): `in 3 days`, `in 3 days`, `in 3d`; `wide` by default |
| `numeric` | `true`: always the number, `1 day ago`; `false`, the default: the word where the language has one, `yesterday` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto pl = txt::locale("pl");
    println("{} | {}", txt::format_relative(-1, txt::time_unit::day, pl),
            txt::format_relative(-1, txt::time_unit::day, pl, {.numeric = true}));
}
```

Output:

```text
wczoraj | 1 dzień temu
```

## See also

- [format_relative](format_relative.md)
- [sgcl::txt](README.md)
