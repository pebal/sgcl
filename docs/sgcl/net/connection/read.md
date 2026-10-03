[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& buffer) const;                                // (1)
async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept;    // (2)
```

Reads what has come into `buffer`, at least one byte and at most its size, waiting until something comes: Go's
`Conn.Read`. Bytes that [read_line](read_line.md) has buffered are taken first.

1. On the calling thread, which waits on the [reactor](../../async/readable.md) meanwhile.
2. The same for a task, which holds no worker while it waits.

Two reads at once are taken one after the other; a read and a write run at once. A read that starts after the
read deadline, or would wait past it, fails and takes nothing, even when data is there
([set_read_deadline](set_read_deadline.md)).

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go; its size is the most read |

## Return value

The number of bytes read, fewer than the size of `buffer` when fewer had come; 0 at the end of the stream, and at
once for an empty `buffer`. Or the [io::error](../../io/error/README.md), its operation `read` and its path the connection
(`tcp 127.0.0.1:50000->127.0.0.1:8080`, a unix socket's path, `pipe` for a pair in memory):

- `io::errc::closed` when the connection was closed before the call, or while it waited ([close](close.md));
- `ETIMEDOUT` when the read deadline passed (`is_timeout()`);
- `ECANCELED` when the reactor stopped while it waited;
- the `errno` of the socket otherwise (`ECONNRESET`).

## Complexity

One system call, linear in the bytes read, and one more per wait for readiness.

## Exceptions

- (1) `std::system_error` when the read has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Notes

The end is not an error: a read of 0 is how the peer says it sent all it will, after its
[close_write](close_write.md) or [close](close.md), as in Go. To fill the whole buffer or fail,
[read_full](read_full.md); to read to the end, [read_all](read_all.md).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    auto [a, b] = net::connection::in_memory();
    auto writing = async::spawn([a]() -> async::task<> {
        co_await a.async_write("hello");
        a.close();
    });
    byte chunk[4];
    for (;;) {
        size_t n = b.read(chunk).value();
        println("read {}", n);
        if (n == 0) {
            break;
        }
    }
    writing.wait();
}
```

Output:

```text
read 4
read 1
read 0
```

A task reads what a TCP client sent:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> show(net::listener l) {
    net::connection c = co_await l.async_accept();
    vector<byte> buffer(64);
    size_t n = (co_await c.async_read(buffer)).value();
    buffer.resize(n);
    println("{} bytes: {}", n, string(buffer));
    c.close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(show(l));
    net::connection c = net::tcp::connect(l.local_endpoint());
    c.write("hello");
    server.wait();
    c.close();
}
```

Output:

```text
5 bytes: hello
```

## See also

- [write, async_write](write.md): the other direction
- [read_full](read_full.md), [read_all](read_all.md), [read_line](read_line.md): a whole buffer, the whole stream, a
  line
- [set_read_deadline](set_read_deadline.md): a limit on the wait
- [sgcl::net::connection](README.md)
