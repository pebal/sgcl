[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](README.md)

# sgcl::io::buffered_reader::discard

```cpp
expected<size_t, error> discard(size_t n) const;
```

Skips the next `n` bytes, fewer at the end of the stream: what the block holds first, then the stream read into the
block and skipped there, a block at a time. It is Go's `bufio.Reader.Discard`.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes to skip |

## Return value

The number of bytes skipped, `n` or fewer at the end of the stream, or the error of the stream's read, as it gave
it.

## Complexity

Linear in `n`: a read of the stream per block skipped.

## Exceptions

What the read of the stream underneath throws: a [file](../file/read.md)'s `std::system_error` when the reactor's
thread cannot be started.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("HEADER--payload"));
    println("{}", *in.discard(8));
    println("{}", *in.read_all_text());
    println("{}", *in.discard(100));  // at the end: none
}
```

Output:

```text
8
payload
0
```

## See also

- [peek](peek.md): the next bytes, not consumed
- [sgcl::io::buffered_reader](README.md)
