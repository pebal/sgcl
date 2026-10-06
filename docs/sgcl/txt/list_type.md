[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::list_type

```cpp
#include "sgcl/txt/list_format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class list_type : uint8_t {
        conjunction,
        disjunction,
        unit,
    };
}
```

What a list joins, as CLDR's list patterns have it ([format_list](format_list.md)).

| Value | Description |
|---|---|
| `conjunction` | all of them: `a, b i c`, `a, b, and c` |
| `disjunction` | one of them: `a, b lub c`, `a, b, or c` |
| `unit` | the parts of one measure: `3 godz., 5 min`, `3 hr, 5 min` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto t :
         {txt::list_type::conjunction, txt::list_type::disjunction, txt::list_type::unit}) {
        println("{}", txt::format_list({"3 h", "5 min", "10 s"}, txt::locale("pl"), t));
    }
}
```

Output:

```text
3 h, 5 min i 10 s
3 h, 5 min lub 10 s
3 h, 5 min i 10 s
```

## See also

- [format_list](format_list.md)
- [sgcl::txt](README.md)
