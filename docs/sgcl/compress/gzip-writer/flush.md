[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip/README.md) › [writer](README.md)

# sgcl::compress::gzip::writer::flush, async_flush

```cpp
expected<void, io::error> flush();                                // (1)
async::task<expected<void, io::error>> async_flush() noexcept;    // (2)
```

Ends the current block and aligns the output with an empty stored block (a *sync flush*, zlib's `Z_SYNC_FLUSH` and
Go's `Flush`), and writes all of it to `out`, so that the other side can decode everything written so far: a stream
of messages, a response sent in parts. It costs a few bytes each time, and the stream goes on: the next write
starts a new block.

1. Blocks the calling thread for the write of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes.

## Parameters

None.

## Return value

Nothing, or the writer's error: a failure of `out` (kept as the first error), a flush after the close
(`io::errc::closed`), or the first error the writer gave before.

## Complexity

Linear in what the encoder holds.

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
    compress::gzip::writer w(sink);
    w.write("first message");
    println("before the flush: {} bytes", sink.size());
    (void)w.flush();
    println("after it: {} bytes", sink.size());

    compress::gzip::reader r(sink);  // the stream is not closed: read what is there
    byte message[13];
    (void)r.read_full(message);
    println("{}", string(slice<const byte>(message)));
}
```

Output:

```text
before the flush: 10 bytes
after it: 29 bytes
first message
```

## See also

- [close](close.md): the end of the stream
- [sgcl::compress::gzip::writer](README.md)
