[sgcl](../../README.md) › [txt](../README.md) › [growing_sink](../growing_sink.md)

# sgcl::txt::growing_sink::size

```cpp
size_t size() const noexcept;
```

What the whole text takes, whether or not it fitted: the [size](../format_sink/size.md) of the sink inside. Compared
with the [capacity](capacity.md) after a step, it says whether the step ran off the end.

## Parameters

None.

## Return value

The number of characters written and counted.

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
    char room[4];
    txt::growing_sink page(room, sizeof room);
    page.out().put("abc");
    println("{} {}", page.size(), page.size() > page.capacity());
    page.out().put("de");
    println("{} {}", page.size(), page.size() > page.capacity());
    return 0;
}
```

Output:

```text
3 false
5 true
```

## See also

- [view](view.md): the characters themselves
- [sgcl::txt::growing_sink](../growing_sink.md)
