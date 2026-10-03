[sgcl](../../README.md) › [io](../README.md) › [mapping](../mapping.md)

# sgcl::io::mapping::close

```cpp
expected<void, error> close() const noexcept;
```

Gives the file back now, for every copy of the handle: the descriptor is closed and the range is replaced by zero
pages (`MAP_FIXED`, one call), so a slice taken before the close reads zeros rather than faulting, and a write
through it stays in that anonymous memory, the file keeping its bytes. After it [data](data.md) is empty,
[size](size.md) is 0 and [flush](flush.md) answers `errc::closed`. The address range itself goes back to the system
with the region, when nothing holds it any more. A second close does nothing. On Windows the handles are closed and
the view stays mapped until the region is collected.

The destructor unmaps a mapping nobody closed, on the collector's thread after the sweep that finds the region
dead: later than the last use. A program that is done with a mapping, and with the file under it, closes it.

## Parameters

None.

## Return value

Nothing, or the [error](../error.md) of closing the descriptor (operation `close`).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("closing.txt", string("closing"));
    io::mapping m = io::map("closing.txt", {.writable = true});
    slice<byte> before = m.writable_data();
    (void)m.close();
    println("{} {} {}", m.is_closed(), m.size(), int(before[0]));
    before[1] = byte('x');  // anonymous memory now
    println("{}", io::read_text("closing.txt").value());
    println("{}", bool(m.close()));
}
```

Output:

```text
true 0 0
closing
true
```

## See also

- [is_closed](is_closed.md): whether the mapping was closed
- [flush](flush.md): the writes on the disk before the close
- [sgcl::io::mapping](../mapping.md)
