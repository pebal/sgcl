[sgcl](../../README.md) › [txt](../README.md) › [html_stencil](README.md)

# sgcl::txt::html_stencil::render_to

```cpp
size_t render_to(const slice<char>& buffer, const value& data) const;
```

Writes the page of `data` into memory the caller lends, as [render](render.md) writes it: what fits is written and
the whole size comes back, whether or not it fitted — the contract of [format_to](../format_to.md).

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | the memory to write into; may be empty |
| `data` | the values |

## Return value

The size of the whole page in bytes; when it is larger than `buffer`, only the first `buffer.size()` bytes were
written.

## Complexity

As [render](render.md).

## Exceptions

What a function of the pipeline throws.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::html_stencil page("<b>{{ x }}</b>");
    char room[64];
    size_t n = page.render_to(room, txt::object{{"x", "a < b"}});
    println("{} {}", n, std::string_view(room, n));
}
```

Output:

```text
15 <b>a &lt; b</b>
```

## See also

- [render](render.md)
- [sgcl::txt::html_stencil](README.md)
