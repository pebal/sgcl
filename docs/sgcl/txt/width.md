[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::width

```cpp
#include "sgcl/txt/locale.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class width : uint8_t {
        wide,
        abbreviated,
        narrow,
    };
}
```

The widths CLDR writes names and phrases in, shared by the lists ([format_list](format_list.md)), relative time
([format_relative](format_relative.md)) and the names of months and days ([time::to_string](../time/to_string.md)).
CLDR calls them wide, abbreviated and narrow for names, standard, short and narrow for lists and long, short and
narrow for relative time: the three are one choice.

| Value | Description |
|---|---|
| `wide` | the whole word: `September`, `a, b, and c`, `in 3 days` |
| `abbreviated` | the short form: `Sep`, `a, b, & c`, `in 3 days` (English has the same here) |
| `narrow` | the least that is still read in context: `S`, `a, b, c`, `in 3d` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto w : {txt::width::wide, txt::width::abbreviated, txt::width::narrow}) {
        println("{} | {}",
                txt::format_list({"a", "b", "c"}, txt::locale("en"), txt::list_type::conjunction,
                                 w),
                txt::format_relative(3, txt::time_unit::day, txt::locale("en"), {.width = w}));
    }
}
```

Output:

```text
a, b, and c | in 3 days
a, b, & c | in 3 days
a, b, c | in 3d
```

## See also

- [format_list](format_list.md)
- [format_relative](format_relative.md)
- [sgcl::txt](README.md)
