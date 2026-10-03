[sgcl](../../README.md) › [txt](../README.md) › [stencil](README.md)

# sgcl::txt::stencil::render

```cpp
string render(const value& data) const;
```

Writes the page of `data`: a program counter over the steps, `.` the root of `data` to begin with, each run of
literal text copied out of the source whole and each field written by [format](../format.md)'s writers. A name the
data does not carry writes nothing, a value put inside itself is `...` where it would be written inside itself, and a
field whose type letter the value does not take is written without it: nothing in the data stops a page.

A page that fits the kilobyte of room kept on the stack is written once, and allocates nothing but the string handed
back. A longer one **grows the room** rather than being written twice: after each step that writes, the walk asks
whether that step ran off the end, and when it did it takes twice as much room, carries over what stood before the
step and writes that one step again ([growing_sink](../growing_sink/README.md)) — never the page; a page of a hundred
kilobytes is one walk and seven doublings, not two walks. So a function of the pipeline may be called twice for one
field, and must be pure ([stencil_function](../stencil_function.md)).

A render allocates for its own bookkeeping **not at all** while the blocks are shallower than four, which is nearly
every template: the parser counts how deep they go and the walk keeps its frames, its dots and its owned values on
the stack. Past that the three move to vectors.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the values: `.` at the top of the page, and `$` everywhere |

## Return value

The page.

## Complexity

Linear in the steps walked — the steps of every block once for each element it walks — and in the length of the
page, plus what the functions of the pipeline cost.

## Exceptions

- `length_error` when the page passes 4 GiB, the most a string holds.
- What a function of the pipeline throws.

## Example

A value built from braces, which read as data:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{
        {"user",   txt::object{{"name", "Ada"}, {"admin", true}}},
        {"scores", txt::list{91.5, 88.0}},
        {"tags",   {"one", "two"}},  // a list inside a mapping needs no type named
        {"count",  7},
    };
    txt::stencil t("{{ user.name }}{{ if user.admin }} (admin){{ end }}: {{ scores }}, "
                   "{{ tags:n }}, {{ count }}");
    println("{}", t.render(data));
    return 0;
}
```

Output:

```text
Ada (admin): [91.5, 88], "one", "two", 7
```

A walk over a mapping, in the order it was written:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"m", txt::object{{"b", 2}, {"a", 1}}}};
    println("{}", txt::stencil("{{ range m }}{{ .key }}={{ .value }} {{ end }}").render(data));
    return 0;
}
```

Output:

```text
b=2 a=1 
```

## See also

- [render_to](render_to.md): into memory the caller lends
- [value](../value/README.md): the data
- [sgcl::txt::stencil](README.md)
