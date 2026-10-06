[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [writer](README.md)

# sgcl::compress::bzip2::writer::flush, async_flush

```cpp
expected<void, io::error> flush();                                // (1)
async::task<expected<void, io::error>> async_flush() noexcept;    // (2)
```

Compresses the block gathered so far and ends the stream, so that the other side can decode everything written:
bzip2 has no way to end a block on a byte inside a stream, so the next write begins another stream, which the readers
(and `bzip2 -d`) read on as one. Each flush costs a stream's header and end (14 bytes) and what a longer block would
have found. A flush with nothing gathered writes nothing.

1. Blocks the calling thread for the work and the write of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes.

## Parameters

None.

## Return value

Nothing, or the writer's error: a failure of `out` (kept as the first error), a flush after the close
(`io::errc::closed`), or the first error the writer gave before.

## Complexity

Linear in what the writer holds.

## Exceptions

- (1) What the `write` of `out` throws; the errors of the stream and of the format are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::bzip2::writer w(sink);
    w.write("first message");
    println("before the flush: {} bytes", sink.size());
    (void)w.flush();
    println("after it: {} bytes", sink.size());

    compress::bzip2::reader r(sink);  // the stream is not closed: read what is there
    byte message[13];
    (void)r.read_full(message);
    println("{}", string(slice<const byte>(message)));
}
```

Output:

```text
before the flush: 0 bytes
after it: 49 bytes
first message
```

## See also

- [close](close.md): the end of the stream
- [sgcl::compress::bzip2::writer](README.md)
