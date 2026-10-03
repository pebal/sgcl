[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::read, async_read

```cpp
expected<size_t, error> read(const slice<byte>& out) const noexcept;                       // (1)
async::task<expected<size_t, error>> async_read(const slice<byte>& out) const noexcept;    // (2)
```

Takes bytes from the front of the buffer into `out`: as many as `out` holds, or as the buffer holds, whichever is
fewer. What is taken is consumed: it is no longer in [data](data.md), and a write position set by a
[seek](seek.md) moves back with it. When the last byte is taken, the bytes are dropped and the memory is kept for
the writes to come.

1. Reads at once.
2. Returns a task that reads at its first step: it never waits.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the bytes go |

## Return value

The number of bytes taken; 0 when the buffer is empty or `out` is. Never an error.

## Complexity

Linear in the bytes taken.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer data("0123456789");
    vector<byte> block(4);
    while (true) {
        auto n = data.read(block);
        if (*n == 0) {
            break;
        }
        println("{} bytes, {} left", *n, data.size());
    }
}
```

Output:

```text
4 bytes, 6 left
4 bytes, 2 left
2 bytes, 0 left
```

## See also

- [data](data.md): the bytes held, without taking them
- [read_full](../mixin/reader/read_full.md), [read_all](../mixin/reader/read_all.md)
- [sgcl::io::buffer](README.md)
