[sgcl](../../README.md) › [txt](../README.md) › [growing_sink](../growing_sink.md)

# sgcl::txt::growing_sink::view

```cpp
std::string_view view() const noexcept;
```

What was written, where it stands, for whoever wants to read it back rather than hand it over: a body about to be
padded into its field, a page about to be written out. What is there and not what was counted: the characters the
room holds, which are [size](size.md) of them for a sink that was given more room whenever it ran out, and only the
ones that fitted for room that was [lent](../growing_sink-lent_t.md). Valid until the next growth.

## Parameters

None.

## Return value

The characters the room holds.

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
    char room[16];
    txt::growing_sink page(room, sizeof room);
    page.out().put("header");
    println("{} {:?}", page.view().size(), page.view());
    return 0;
}
```

Output:

```text
6 "header"
```

## See also

- [text](text.md): the same as a string
- [sgcl::txt::growing_sink](../growing_sink.md)
