[sgcl](../../README.md) › [txt](../README.md) › [html_stencil](README.md)

# sgcl::txt::html_stencil::html_stencil

```cpp
html_stencil() = default;                                                           // (1)
explicit html_stencil(const string& source);                                        // (2)
explicit html_stencil(const string& source, const stencil_functions& functions);    // (3)
```

Constructs a template.

1. The empty template: it writes nothing.
2. The template a literal of the program spells, read as [parse](parse.md) reads it, with the six functions every
   template has and the five `safe_` ones.
3. The same with a table of the caller's, which the `safe_` functions are added to.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the template |
| `functions` | the functions it may call |

## Complexity

Linear in the length of the source.

## Exceptions

`bad_expected_access<stencil_error>` with the error of [parse](parse.md) when `source` is not a template (2, 3).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::html_stencil page(R"(<a href="{{ url }}">{{ name }}</a>)");
    println("{}", page.render(txt::object{{"url", "/u?id=7&x=y z"}, {"name", "<Ada>"}}));
}
```

Output:

```text
<a href="/u?id=7&amp;x=y%20z">&lt;Ada&gt;</a>
```

## See also

- [parse](parse.md)
- [sgcl::txt::html_stencil](README.md)
