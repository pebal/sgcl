[sgcl](../../README.md) › [txt](../README.md) › [stencil](README.md)

# sgcl::txt::stencil::render_to

```cpp
size_t render_to(const slice<char>& buffer, const value& data) const;
```

Writes the page of `data` into memory the caller lends, as [render](render.md) writes it: what fits is written and
the whole size comes back, whether or not it fitted — the contract of [format_to](../format_to.md) — so a caller may
ask with an empty buffer and then size one. The room is not grown and nothing is allocated for the page: each step is
written once.

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
    txt::stencil row("{{ name:<6 }}|{{ score:>5.1f }}");
    char room[32];
    for (auto [name, score] : {pair{"Ada", 91.5}, pair{"Alan", 88.0}}) {
        size_t n = row.render_to(room, txt::object{{"name", name}, {"score", score}});
        println("{} ({} bytes)", std::string_view(room, n), n);
    }
    return 0;
}
```

Output:

```text
Ada   | 91.5 (12 bytes)
Alan  | 88.0 (12 bytes)
```

## See also

- [render](render.md): into a string
- [sgcl::txt::stencil](README.md)
