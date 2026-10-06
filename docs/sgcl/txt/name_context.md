[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::name_context

```cpp
#include "sgcl/txt/locale.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class name_context : uint8_t {
        format,
        standalone,
    };
}
```

The form of a name of CLDR: the one it takes inside a phrase, or the one it takes standing alone, in a list, a menu
or a heading. Languages that inflect their names tell the two apart — Polish "24 września" and "wrzesień", Russian
"24 сентября" and "сентябрь" — and for the others they are one. [time::to_string](../time/to_string.md) takes it for
the names of months and days.

| Value | Description |
|---|---|
| `format` | the form inside a phrase: `września` |
| `standalone` | the form standing alone: `wrzesień` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto c : {txt::name_context::format, txt::name_context::standalone}) {
        println("{}",
                time::to_string(time::month::september, txt::locale("pl"), txt::width::wide, c));
    }
}
```

Output:

```text
września
wrzesień
```

## See also

- [width](width.md)
- [time::to_string](../time/to_string.md)
- [sgcl::txt](README.md)
