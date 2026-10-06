[sgcl](../../README.md) › [txt](../README.md) › [html_stencil](README.md)

# sgcl::txt::html_stencil::parses

```cpp
static bool parses(const string& source) noexcept;
```

Checks whether a source is a template whose fields all have a safe place, with nothing kept.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the template |

## Return value

`true` when [parse](parse.md) would read it.

## Complexity

Linear in the length of the source.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::html_stencil::parses("<b>{{ x }}</b>"),
            txt::html_stencil::parses("<a href=\"{{ x }}"));
}
```

Output:

```text
true false
```

## See also

- [parse](parse.md)
- [sgcl::txt::html_stencil](README.md)
