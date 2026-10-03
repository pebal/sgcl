[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib.md) › [writer](../zlib-writer.md)

# sgcl::compress::zlib::writer::close, async_close

```cpp
expected<void, io::error> close();                                // (1)
async::task<expected<void, io::error>> async_close() noexcept;    // (2)
```

Ends the stream: writes the last block and the Adler-32 of the data to `out`, and leaves `out` open, for a format or a
protocol that goes on after it; the program closes `out` itself. A writer closed is [is_closed](is_closed.md), and a
write or a flush after it is `io::errc::closed`. A second close after a close that succeeded does nothing and succeeds.

The close is where a stream written freely is checked once: it gives the first error the writer gave, of any write
or flush before it, and writes nothing then.

1. Blocks the calling thread for the write of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes.

## Parameters

None.

## Return value

Nothing, or the writer's first error: a failure of `out`, a write or a flush after a close.

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
    compress::zlib::writer w(sink);
    w.write("hello, hello, hello");
    auto done = w.close();
    println("{} {} {} bytes", done.has_value(), w.is_closed(), sink.size());

    auto late = w.write("more");
    println("{}", late.error().message());
    println("{}", w.close().error().message());
}
```

Output:

```text
true true 16 bytes
write zlib: stream closed
write zlib: stream closed
```

## See also

- [flush](flush.md): everything so far decodable, the stream going on
- [last_error](last_error.md): the error the close gives
- [sgcl::compress::zlib::writer](../zlib-writer.md)
