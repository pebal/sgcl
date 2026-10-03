[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](README.md)

# sgcl::io::buffered_reader::peek

```cpp
expected<slice<const byte>, error> peek(size_t n) const;
```

Returns the next `n` bytes without consuming them: fewer at the end of the stream, and at most the block's size,
`config::io_buffer_size` (8 KB). The stream is read as many times as it takes to have them in the block, the unread
bytes moved to its front first when they do not fit behind. The bytes are a slice of the block, which holds it: valid
until the next read, as a line is. It is Go's `bufio.Reader.Peek`, which fails with `ErrBufferFull` for more than its
buffer, where this gives the block's worth.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes to look at |

## Return value

The bytes, `min(n, 8 KB)` of them or fewer at the end of the stream (none at its end), or the error of the stream's
read, as it gave it.

## Complexity

Linear in `n`; the reads of the stream it takes.

## Exceptions

What the read of the stream underneath throws: a [file](../file/read.md)'s `std::system_error` when the reactor's
thread cannot be started.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("%PDF-1.7 and the rest"));
    auto magic = in.peek(5);
    println("{}", string(*magic) == "%PDF-");
    println("[{}]", **in.read_until(' '));  // nothing was consumed
}
```

Output:

```text
true
[%PDF-1.7 ]
```

## See also

- [discard](discard.md): skips bytes
- [read_byte](read_byte.md): one byte, consumed
- [sgcl::io::buffered_reader](README.md)
