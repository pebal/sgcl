[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw.md) › [writer](../lzw-writer.md)

# sgcl::compress::lzw::writer::close, async_close

```cpp
/*(1)*/ expected<void, io::error> close();
/*(2)*/ async::task<expected<void, io::error>> async_close() noexcept;
```

Ends the stream: writes the last code and the end code to `out`, and leaves `out` open, as Go's does; the program
closes `out` itself. A writer closed is [is_closed](is_closed.md), and a write after it is `io::errc::closed`. A
second close after a close that succeeded does nothing and succeeds. After an error kept, the close gives it and
writes nothing.

1. Blocks the calling thread for the write of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes.

## Parameters

None.

## Return value

Nothing, or the writer's error: a failure of `out`, or the error kept from a write (one after a close among them).

## Complexity

Constant.

## Exceptions

- (1) What the `write` of `out` throws; the errors of the stream are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::lzw::writer w(sink, compress::lzw::order::lsb, 8);
    w.write("hello");
    println("{} bytes before the close", sink.size());
    println("{}", w.close().has_value());
    println("{} bytes after it", sink.size());
    println("{}", w.close().has_value());
}
```

Output:

```text
5 bytes before the close
true
8 bytes after it
true
```

## See also

- [is_closed](is_closed.md)
- [sgcl::compress::lzw::writer](../lzw-writer.md)
