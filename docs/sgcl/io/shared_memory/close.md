[sgcl](../../README.md) › [io](../README.md) › [shared_memory](README.md)

# sgcl::io::shared_memory::close

```cpp
expected<void, error> close() const noexcept;
```

Gives the region back now, for every copy of the handle, as [mapping::close](../mapping/close.md) does: the
descriptor (the handles on Windows) is closed and the range is replaced by zero pages, so a slice taken before reads
zeros rather than faulting, and a write through it stays in that anonymous memory, the object keeping its bytes.
After it [data](data.md) is empty and [size](size.md) 0. The object and its name are untouched: another
[open](open.md) maps it again, and [remove](remove.md) takes the name away. A second close does nothing.

The destructor unmaps a region nobody closed, on the collector's thread after the sweep that finds it dead.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md) of closing the descriptor (operation `close`).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::shared_memory::remove("sgcl-example-close");
    io::shared_memory region = io::shared_memory::create("sgcl-example-close", 64);
    slice<byte> before = region.data();
    before[0] = byte(7);
    (void)region.close();
    println("{} {} {}", region.is_closed(), region.size(), int(before[0]));
    before[1] = byte(9);  // anonymous memory now
    io::shared_memory again = io::shared_memory::open("sgcl-example-close");
    println("{} {}", int(again.data()[0]), int(again.data()[1]));
    (void)io::shared_memory::remove("sgcl-example-close");
}
```

Output:

```text
true 0 0
7 0
```

## See also

- [is_closed](is_closed.md): whether the region was closed
- [remove](remove.md): the name taken away
- [sgcl::io::shared_memory](README.md)
