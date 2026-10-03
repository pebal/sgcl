[sgcl](../../README.md) › [txt](../README.md) › [growing_sink](../growing_sink.md)

# sgcl::txt::growing_sink::out

```cpp
format_sink& out() noexcept;
```

The sink the values go into, the one `format_to`, `write_padded` and a `format_value` write with. The reference stays
good over a growth: the sink is this object's own and is moved over the new room where it stands, so a caller may
take it once for the whole walk.

## Parameters

None.

## Return value

The sink.

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
    txt::format_sink& out = page.out();
    size_t cap = page.capacity();
    for (int row : {1, 2, 3}) {
        size_t mark = page.size();
        txt::write_padded(out, "row", txt::format_spec{.align = '>', .width = 5});
        out.put(char('0' + row));
        if (page.size() > cap) [[unlikely]] {
            cap = page.take_room(page.size(), mark);
            txt::write_padded(out, "row", txt::format_spec{.align = '>', .width = 5});
            out.put(char('0' + row));
        }
    }
    println("[{}]", page.view());
    return 0;
}
```

Output:

```text
[  row1  row2  row3]
```

## See also

- [format_sink](../format_sink.md): what it is
- [sgcl::txt::growing_sink](../growing_sink.md)
