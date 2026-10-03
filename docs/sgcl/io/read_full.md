[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_full, async_read_full

```cpp
#include "sgcl/io/functions.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ template<req::reader R>
            expected<size_t, error> read_full(R&& r, const slice<byte>& buffer)
                noexcept(/* see below */);
    /*(2)*/ template<req::async_reader R>
            async::task<expected<size_t, error>> async_read_full(R&& r, const slice<byte>& buffer)
                noexcept(/* see below */);
}
```

Fills the whole of `buffer` from the stream `r`, reading as many times as it takes: a stream may give fewer bytes
than asked at each read, a socket or a pipe what has come so far. Go's `io.ReadFull`, its two ends told apart as
there: the stream ending before the first byte is the end of the stream, a result of 0 as a read's (Go's `io.EOF`),
which a loop over records of a fixed size ends on; ending part way is the error `errc::unexpected_eof`, which
carries the number of bytes read (Go's `io.ErrUnexpectedEOF` beside `n`).

1. Reads on the calling thread, with the stream's `read`.
2. The same for a task, with the stream's `async_read`: the task gives its worker back while it waits. A stream
   given as a temporary is moved into the task's frame; one given by reference is the caller's to keep alive until
   the task is done, which a `co_await` in the same statement does by itself.

`r` is any [reader](req/reader.md) (2: async reader): a stream of the library, a class of
the program's with the method, a callable of the same shape, a reference or a `tracked_ptr` to one. A class that
carries [mixin::reader](mixin/reader.md) has the same as a member, [r.read_full(buffer)](mixin/reader/read_full.md).

## Parameters

| Parameter | Description |
|---|---|
| `r` | the stream to read |
| `buffer` | the bytes to fill, all of them |

## Return value

`buffer.size()`; 0 when the stream ends before the first byte, or when `buffer` is empty. Or the error:

- `errc::unexpected_eof` (`is_eof()`) when the stream ends part way: the bytes read until then are at the front of
  `buffer`, and their number is the error's [count](error/count.md);
- the error of a read of `r`, as it gave it.

## Complexity

Linear in `buffer.size()`; the number of reads is the stream's.

## Exceptions

- (1) What the read of `r` throws; none when it is noexcept, as an `io::buffer`'s is, and then `read_full` is
  declared noexcept.
- (2) What the move of a stream given as a temporary into the task's frame throws; none for a stream given by
  reference. What a read throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer records("ab12cd34");  // records of four bytes
    array<byte, 4> record;
    for (;;) {
        auto n = io::read_full(records, record).value();
        if (n == 0) {
            break;
        }
        println("{} bytes: {}", n, string(record));
    }

    io::buffer cut("ef");  // a record cut short
    auto short_one = io::async_read_full(cut, record).wait();
    println("{}, {} bytes read", short_one.error().message(), short_one.error().count());
}
```

Output:

```text
4 bytes: ab12
4 bytes: cd34
read: unexpected end of stream, 2 bytes read
```

## See also

- [read_all](read_all.md): everything to the end of the stream
- [mixin::reader::read_full](mixin/reader/read_full.md): the same as a member of a stream
- [req::reader, async_reader](req/reader.md): what `r` may be
- [errc](errc.md): `unexpected_eof`
- [error::count](error/count.md): the bytes read before the end
