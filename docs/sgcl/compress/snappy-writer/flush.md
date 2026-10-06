[sgcl](../../README.md) › [compress](../README.md) › [snappy](../snappy/README.md) › [writer](README.md)

# sgcl::compress::snappy::writer::flush, async_flush

```cpp
expected<void, io::error> flush();                                // (1)
async::task<expected<void, io::error>> async_flush() noexcept;    // (2)
```

Compresses the chunk gathered so far and writes it to `out`, so that the other side can decode everything written:
a stream of messages, a response sent in parts. Each flush ends a chunk early, which costs its header (eight bytes)
and what a longer chunk would have found; chunks are independent, so nothing after it refers to the data before. A flush with nothing gathered writes nothing.

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
    compress::snappy::writer w(sink);
    w.write("first message");
    println("before the flush: {} bytes", sink.size());
    (void)w.flush();
    println("after it: {} bytes", sink.size());

    compress::snappy::reader r(sink);  // the stream is not closed: read what is there
    byte message[13];
    (void)r.read_full(message);
    println("{}", string(slice<const byte>(message)));
}
```

Output:

```text
before the flush: 10 bytes
after it: 31 bytes
first message
```

## See also

- [close](close.md): the end of the stream
- [sgcl::compress::snappy::writer](README.md)
