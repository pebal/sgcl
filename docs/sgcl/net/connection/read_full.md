[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::read_full, async_read_full

```cpp
expected<size_t, io::error> read_full(const slice<byte>& buffer) const;                                // (1)
async::task<expected<size_t, io::error>> async_read_full(const slice<byte>& buffer) const noexcept;    // (2)
```

Fills the whole of `buffer` from the connection, reading as many times as it takes: Go's `io.ReadFull(c, b)`. The
stream ending before the first byte is the end of the stream, a result of 0 (Go's `io.EOF`); ending part way is
`io::errc::unexpected_eof`, which carries the number of bytes read. It is [io::read_full](../../io/read_full.md) over
the connection.

1. On the calling thread.
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | the bytes to fill, all of them |

## Return value

`buffer.size()`; 0 when the stream ends before the first byte; or the [io::error](../../io/error/README.md):
`io::errc::unexpected_eof` (`is_eof()`), its operation `read`, when the stream ends part way, the bytes read until
then at the front of `buffer` and their number in the error's [count](../../io/error/count.md); the error of a
[read](read.md) otherwise, as the read gave it.

## Complexity

Linear in `buffer.size()`; the number of reads is the network's.

## Exceptions

- (1) `std::system_error` when a read has to wait and the thread of the reactor, or of the timers, which its first
  use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto [a, b] = net::connection::in_memory();
    auto writing = async::spawn([a]() -> async::task<> {
        co_await a.async_write("head");
        co_await a.async_write("er and body");
        a.close();
    });
    vector<byte> header(6);
    println("{}", b.read_full(header).value());
    println("{}", string(header));

    vector<byte> rest(100);
    auto r = b.read_full(rest);
    println("{} {} {}", r.error().message(), r.error().is_eof(), r.error().count());
    writing.wait();
}
```

Output:

```text
6
header
read: unexpected end of stream true 9
```

## See also

- [read, async_read](read.md): what has come, at least one byte
- [read_all](read_all.md): everything to the end
- [sgcl::net::connection](README.md)
