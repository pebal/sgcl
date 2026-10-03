[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::close, async_close

```cpp
expected<void, io::error> close() const noexcept;                       // (1)
async::task<expected<void, io::error>> async_close() const noexcept;    // (2)
```

Ends the connection now, both ways, from any thread or task: Go's `Conn.Close`. The reads, writes and waits in
progress in other tasks end with `io::errc::closed`, and no operation starts after it; a second close does nothing
and succeeds. A close from another task is the way to cancel what waits on a connection.

The descriptor goes back to the system when the last operation in progress has let go of it, never under one:
every operation holds the descriptor while it runs, and the `::close` is made by the last of them to let go, so an
operation never lands on a number the kernel has given to another socket meanwhile. A connection not closed is
closed by its destructor, on the collector's thread after the sweep that finds it dead: later than the last use. A
connection that is done is closed.

1. On the calling thread, and it never waits: over [TLS](../tls/README.md) the closing record (`close_notify`) goes
   only when the socket takes it at once.
2. The same for a task. Over TLS the closing record is written first, given up to 100 ms, and the task waits for
   it; a socket's close and a pair's return at once.

## Parameters

None.

## Return value

Nothing, or the [io::error](../../io/error.md) of the close, its operation `close` and its path the connection.

## Complexity

Constant: one system call, or none when an operation in progress makes it.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> wait_for_data(net::connection c) {
    byte b[16];
    auto n = co_await c.async_read(b);  // nothing is written: it waits
    println("{} {}", n.error().message(), n.error().is_closed());
}

int main() {
    auto [a, b] = net::connection::in_memory();
    auto waiting = async::spawn(wait_for_data(b));
    async::sleep(10ms).wait();
    b.close();
    waiting.wait();
    println("{} {}", static_cast<bool>(b.close()), b.is_closed());
}
```

Output:

```text
read pipe: stream closed true
true true
```

## See also

- [close_write](close_write.md): the writing half alone
- [is_closed](is_closed.md): whether the connection was closed
- [set_deadline](set_deadline.md): an end to the waits without a close
- [sgcl::net::connection](../connection.md)
