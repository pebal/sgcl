[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::value_kind

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class value_kind : uint8_t {
        none,
        boolean,
        integer,
        real,
        text,
        list,
        object,
    };
}
```

What a [value](value/README.md) holds, as its [kind](value/kind.md) answers. Scoped, so that `list` and `object` here and the
two classes of those names do not have to fight over the words: a `value_kind::list` is what a [list](list/README.md) makes.

| Value | Description |
|---|---|
| `none` | nothing: a value made with no arguments or of `nullptr`, and what a name nobody gave answers to |
| `boolean` | `true` or `false` |
| `integer` | a whole number, held as a `long long` |
| `real` | a floating-point number, held as a `double` |
| `text` | a [string](../core/string/README.md) |
| `list` | a list of values, in order |
| `object` | a mapping of names to values, in the order they were written |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (txt::value v : {txt::value(), txt::value(true), txt::value(7), txt::value(2.5),
                         txt::value("seven"), txt::value(txt::list{1, 2}),
                         txt::value(txt::object{})}) {
        print("{} ", int(v.kind()));
    }
    println("{}", txt::value("seven").kind() == txt::value_kind::text);
    return 0;
}
```

Output:

```text
0 1 2 3 4 5 6 true
```

## See also

- [kind](value/kind.md): what a value holds
- [value](value/README.md)
