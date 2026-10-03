[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::buffered

```cpp
size_t buffered() const noexcept;
```

Returns the number of bytes in the block not yet read: what the next reads take without touching the stream. It is
Go's `bufio.Reader.Buffered`.

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
    io::buffered_reader in(io::buffer("one\ntwo\n"));
    println("{}", in.buffered());  // nothing read yet
    in.read_line();
    println("{}", in.buffered());  // "two\n", read with the first line
}
```

Output:

```text
0
4
```

## See also

- [peek](peek.md): the next bytes, not consumed
- [sgcl::io::buffered_reader](../buffered_reader.md)
