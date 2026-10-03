[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::read_byte

```cpp
expected<optional<byte>, error> read_byte() const;
```

Reads one byte, from the block, refilled by one read of the stream when it is empty. It is Go's
`bufio.Reader.ReadByte`, with the end of the stream as an empty `optional` instead of `io.EOF`. A byte at a time
costs a call, not a read of the stream: the stream is read once per block.

## Parameters

None.

## Return value

The byte, an empty `optional` at the end of the stream, or the error of the stream's read, as it gave it.

## Complexity

Constant; a read of the stream when the block is empty.

## Exceptions

What the read of the stream underneath throws: a [file](../file/read.md)'s `std::system_error` when the reactor's
thread cannot be started.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("a1b2"));
    int digits = 0;
    while (auto b = in.read_byte()) {
        if (!*b) {
            break;  // the end of the stream
        }
        char c = char(**b);
        digits += c >= '0' && c <= '9';
    }
    println("{}", digits);
}
```

Output:

```text
2
```

## See also

- [peek](peek.md): the next bytes, not consumed
- [read](read.md): bytes into a buffer
- [sgcl::io::buffered_reader](../buffered_reader.md)
