[sgcl](../../README.md) › [txt](../README.md) › [html_stencil](README.md)

# sgcl::txt::html_stencil::source

```cpp
const string& source() const noexcept;
```

Returns the text the template was read from.

## Parameters

None.

## Return value

The source; an empty text for the empty template.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::html_stencil page("<b>{{ x }}</b>");
    println("{}", page.source());
}
```

Output:

```text
<b>{{ x }}</b>
```

## See also

- [parse](parse.md)
- [sgcl::txt::html_stencil](README.md)
