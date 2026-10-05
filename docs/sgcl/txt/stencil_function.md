[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::stencil_function

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    using stencil_function = function<value(const value&, slice<const value>)>;
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

A function of the pipeline of a [stencil](stencil/README.md): `{{ name | upper }}`, `{{ n | default 0 }}`. What comes down
the pipe is the first argument, and whatever was written after the name is the second — literals and paths both,
already worked out, at most eight. A function answers a [value](value/README.md), so functions compose and one may hand a
list to the next. It is a [function](../core/function/README.md) of the library, so a lambda converts to it; it is named in
a table of [stencil_functions](stencil_functions/README.md), which [parse](stencil/parse.md) resolves every name against.

## Rules

- **It must be pure of side effects.** A render that does not fit the room it was given writes one step again — one
  field, into room twice the size, rather than the page — so the pipeline of that field runs a second time, path and
  all, and a function may be called twice for the same field. Which field it is depends on where the page happens to
  cross a kilobyte and each doubling after it, so a function that counts, logs or reads a clock is right on most
  pages and wrong on some, and the some are not the ones anybody tests. Nothing else in a render depends on it.
- What it throws, a [render](stencil/render.md) throws.

## Example

A function that counts its calls, which a pure function must not do, shows when a field is written again:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    int calls = 0;
    txt::stencil_functions table;
    table.add("counted", [&](const txt::value& v, slice<const txt::value>) {
        ++calls;
        return v;
    });
    auto t = txt::stencil::parse("{{ a | counted }}{{ b | counted }}{{ c | counted }}", table);
    for (size_t size : {303, 343, 703}) {
        calls = 0;
        string part(size, 'x');
        size_t page = t->render(txt::object{{"a", part}, {"b", part}, {"c", part}}).size();
        println("a page of {:>4} characters, three fields: the function ran {} times", page, calls);
    }
    return 0;
}
```

Output:

```text
a page of  909 characters, three fields: the function ran 3 times
a page of 1029 characters, three fields: the function ran 4 times
a page of 2109 characters, three fields: the function ran 5 times
```

## See also

- [stencil_functions](stencil_functions/README.md): the table
- [stencil](stencil/README.md#the-pipeline): the pipeline
