[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](README.md)

# sgcl::io::buffered_writer::available

```cpp
size_t available() const noexcept;
```

Returns the room left in the block: the bytes a write takes before the block is full and goes to the stream. It is
Go's `bufio.Writer.Available`; [buffered](buffered.md) and `available()` add up to the block's size,
`config::io_buffer_size` (8 KB).

## Parameters

None.

## Return value

The number of bytes the block has room for.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    io::buffered_writer w(out);
    w.write("12345");
    println("{} + {} = {}", w.buffered(), w.available(), w.buffered() + w.available());
}
```

Output:

```text
5 + 8187 = 8192
```

## See also

- [buffered](buffered.md): the bytes not yet written
- [sgcl::io::buffered_writer](README.md)
