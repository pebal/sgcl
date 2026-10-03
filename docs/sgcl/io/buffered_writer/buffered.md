[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::buffered_writer::buffered

```cpp
size_t buffered() const noexcept;
```

Returns the number of bytes in the block not yet written to the stream: what a [flush](flush.md) would write. It is
Go's `bufio.Writer.Buffered`.

## Parameters

None.

## Return value

The number of bytes buffered, at most `config::io_buffer_size` (8 KB).

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
    println("{} {}", w.buffered(), out.size());
    w.flush();
    println("{} {}", w.buffered(), out.size());
}
```

Output:

```text
5 0
0 5
```

## See also

- [available](available.md): the room left
- [flush](flush.md): writes what is buffered
- [sgcl::io::buffered_writer](../buffered_writer.md)
