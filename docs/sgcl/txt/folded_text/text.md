[sgcl](../../README.md) › [txt](../README.md) › [folded_text](README.md)

# sgcl::txt::folded_text::text

```cpp
const slice<const char>& text() const noexcept;
```

Returns the text the object was built from, as it was given: the positions [find](find.md) answers with are bytes
of it. A C text is the copy the object holds.

## Parameters

None.

## Return value

The slice of the text; an empty slice for an object made by the default constructor.

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
    txt::folded_text ft("Ein Gruß aus München");
    auto o = ft.find("GRUSS");
    println("{}", ft.text().subslice(o->pos, o->size));
}
```

Output:

```text
Gruß
```

## See also

- [find](find.md): positions in this text
- [sgcl::txt::folded_text, normalized_text](README.md)
