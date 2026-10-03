[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::read, async_read

```cpp
expected<size_t, error> read(const slice<byte>& out) const;                                // (1)
async::task<expected<size_t, error>> async_read(const slice<byte>& out) const noexcept;    // (2)
```

Reads bytes into `out`: what the block holds first, as much of it as fits. When the block is empty it is refilled
by one read of the stream underneath, and a read of at least the block's size (8 KB) into an empty block goes to the
stream directly, into `out`, with no copy through the block. One call gives at most what the block held or one read
of the stream gave, as Go's `bufio.Reader.Read`: [read_full](../mixin/reader/read_full.md) fills `out` whole.

1. Reads on the calling thread.
2. The same for a task: the stream's `async_read`, or its `read` on the blocking pool for a stream that has only
   that ([io::reader](../reader.md)). The reader is held by the handle the task was made from: the caller keeps it
   alive until the task is done.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the bytes go |

## Return value

The number of bytes put at the front of `out`, 0 at the end of the stream (and for an empty `out`), or the error of
the stream's read, as it gave it.

## Complexity

Linear in the number of bytes copied; at most one read of the stream.

## Exceptions

- (1) What the read of the stream underneath throws: a [file](../file/read.md)'s `std::system_error` when the
  reactor's thread cannot be started.
- (2) None. What the read throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("abcdefghij"));
    vector<byte> four(4);
    auto n = in.read(four);
    println("{} {}, {} left in the block", *n, string(four), in.buffered());

    vector<byte> more(100);
    println("{}", *in.read(more));  // what the block held, no more
    println("{}", *in.async_read(more).wait());  // the end
}
```

Output:

```text
4 abcd, 6 left in the block
6
0
```

## See also

- [read_byte](read_byte.md): one byte
- [read_line](read_line.md): a line, as a slice of the block
- [read_full](../mixin/reader/read_full.md): the whole buffer
- [sgcl::io::buffered_reader](../buffered_reader.md)
