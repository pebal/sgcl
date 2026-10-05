[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::list

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class list : public value;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

A list of [values](../value/README.md) written as data: `txt::list{1, 2, 3}`, `txt::list{91.5, "n/a", true}`. It is a `value`
and adds nothing to it — no member, nothing virtual — so it is a value wherever one is wanted and the slicing that
gives is the point rather than a hazard. It exists because a brace on its own cannot say whether a list or a mapping
was meant. A template walks it with `range`, `.` each element in turn; `format` writes it in brackets, its text
elements in quotes, as C++23 writes a range.

## Rules

- What a [value](../value/README.md) holding a list is: a copy shares the elements.
- `txt::list{}`, with no elements, is an empty list, written `[]`: a `range` over it takes the empty road and an `if`
  finds it false.
- [format](../format.md) writes it as the value it is: `txt::format("{}", xs)`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](list.md) | makes the list |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value scores = txt::list{91.5, 88, "n/a"};
    txt::stencil t("{{ range . }}<{{ . }}>{{ end }} {{ .:n }} {{ . }}");
    println("{}", t.render(scores));
    return 0;
}
```

Output:

```text
<91.5><88><n/a> 91.5, 88, "n/a" [91.5, 88, "n/a"]
```

## See also

- [object](../object/README.md): a mapping
- [value](../value/README.md)
