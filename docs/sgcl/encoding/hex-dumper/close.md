[sgcl](../../README.md) › [encoding](../README.md) › [hex](../hex.md) › [dumper](../hex-dumper.md)

# sgcl::encoding::hex::dumper::close, async_close

```cpp
/*(1)*/ expected<void, io::error> close() const;
/*(2)*/ async::task<expected<void, io::error>> async_close() const noexcept;
```

Writes the short line at the end, the bytes that waited for sixteen, with the columns where a whole line has
them, and ends the dumper: a write after it is `io::errc::closed`. Nothing is written when no bytes wait. The
writer under it stays open, as Go's `Dumper` leaves it. A second `close()` writes nothing and succeeds.

1. Waits on this thread as the writer under it does.
2. The same in a task, `co_await wire.async_close()`, over the writer's `async_write`.

A failure of the writer under it, in this close or in a write before, is reported, and the dumper is closed.

## Parameters

None.

## Return value

Nothing, or the `io::error` of the writer under it.

## Complexity

Constant.

## Exceptions

- (1) What the write of the writer under it throws; the writers of the library throw nothing.
- (2) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::hex::dumper wire = encoding::hex::dumper_to(io::stdout);
    wire.write("twenty bytes, a line");
    wire.close();
    println("{}", wire.write("more").error().message());
}
```

Output:

```text
00000000  74 77 65 6e 74 79 20 62  79 74 65 73 2c 20 61 20  |twenty bytes, a |
00000010  6c 69 6e 65                                       |line|
write hex dump: stream closed
```

## See also

- [write, async_write](write.md): bytes into the dumper
- [is_closed](is_closed.md): whether the dumper was closed
- [sgcl::encoding::hex::dumper](../hex-dumper.md)
