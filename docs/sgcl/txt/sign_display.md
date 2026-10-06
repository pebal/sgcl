[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::sign_display

```cpp
#include "sgcl/txt/number.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class sign_display : uint8_t {
        automatic,
        always,
        never,
        except_zero,
        negative,
    };
}
```

Which numbers a [number_format](number_format/README.md) writes a sign for. A positive sign takes the place of the
locale's negative one, and a pattern whose negative form has no minus (the parentheses of accounting) gets the plus
in front.

| Value | Description |
|---|---|
| `automatic` | `-1`, `0`, `1`; a negative value that rounds to zero keeps its minus, `-0`, as ICU writes it |
| `always` | `-1`, `+0`, `+1` |
| `never` | `1`, `0`, `1` |
| `except_zero` | `-1`, `0`, `+1` |
| `negative` | `-1`, `0`, `1`, and no minus on a value that rounds to zero |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s :
         {txt::sign_display::automatic, txt::sign_display::always, txt::sign_display::never,
          txt::sign_display::except_zero, txt::sign_display::negative}) {
        txt::number_format f(txt::locale("en"), {.max_fraction = 0, .sign = s});
        println("{} {} {} {}", f.format(-1), f.format(-0.2), f.format(0), f.format(1));
    }
}
```

Output:

```text
-1 -0 0 1
-1 -0 +0 +1
1 0 0 1
-1 0 0 +1
-1 0 0 1
```

## See also

- [number_options](number_options.md)
- [sgcl::txt](README.md)
