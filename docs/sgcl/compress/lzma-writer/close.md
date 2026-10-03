[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma.md) › [writer](../lzma-writer.md)

# sgcl::compress::lzma::writer::close, async_close

```cpp
expected<void, io::error> close();                                // (1)
async::task<expected<void, io::error>> async_close() noexcept;    // (2)
```

Ends the stream: codes what is left in the window and writes the end marker and the range coder's last bytes to `out`,
and leaves `out` open; the program closes `out` itself. A writer closed is [is_closed](is_closed.md), and a write after
it is `io::errc::closed`. A second close after a close that succeeded does nothing and succeeds. The close gives the
first error the writer gave, of any write before it, and writes nothing then.

1. Blocks the calling thread for the work and the writes of `out`.
2. Returns a task that does the same in portions of 64 KB with a yield between them.

## Parameters

None.

## Return value

Nothing, or the writer's first error: a failure of `out`, options out of range, a write after a close.

## Complexity

Linear in what the window holds.

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
    compress::lzma::writer w(sink);
    w.write("hello, hello, hello");
    auto done = w.close();
    println("{} {} {} bytes", done.has_value(), w.is_closed(), sink.size());
    println("{}", w.write("more").error().message());
}
```

Output:

```text
true true 32 bytes
write lzma: stream closed
```

## See also

- [last_error](last_error.md): the error the close gives
- [sgcl::compress::lzma::writer](../lzma-writer.md)
