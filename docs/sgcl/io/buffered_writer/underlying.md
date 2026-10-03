[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::buffered_writer::underlying

```cpp
io::writer underlying() const noexcept;
```

Returns the stream the writer writes through its block, as the [io::writer](../writer.md) it was made of: a copy of
that handle, the same stream. A write to it goes past the block, ahead of the bytes the block still holds.

## Parameters

None.

## Return value

The stream underneath.

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
    w.write("second");
    w.underlying().write("first, ");  // past the block
    w.flush();
    println("{}", out.text());
}
```

Output:

```text
first, second
```

## See also

- [io::writer](../writer.md): what it is
- [flush](flush.md): writes the block to it
- [sgcl::io::buffered_writer](../buffered_writer.md)
