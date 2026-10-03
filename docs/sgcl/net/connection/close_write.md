[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::close_write

```cpp
expected<void, io::error> close_write() const;
```

Ends the writing half of the connection, `shutdown(SHUT_WR)`: the peer reads the end of the stream, a read of 0,
and this side can still read what the peer sends. Go's `CloseWrite` of a `*net.TCPConn`. A pair in memory does the
same: the other end reads the end, and a write of this end fails. The connection itself stays open until
[close](close.md).

## Parameters

None.

## Return value

Nothing; or the [io::error](../../io/error.md), its operation `close_write`: `io::errc::closed` on a connection
already closed, the `errno` of `shutdown` otherwise.

## Complexity

Constant: one system call.

## Exceptions

None for a socket and a pair in memory. Over [TLS](../tls/README.md), which writes its closing record
(`close_notify`) first and waits for the socket to take it, `std::system_error` when the thread of the reactor, or
of the timers, which its first use starts, cannot be made.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

// Reads the whole request, to its end, then answers
async::task<> count(net::listener l) {
    net::connection c = co_await l.async_accept();
    string request = (co_await c.async_read_all_text()).value();
    co_await c.async_write(to_string(request.size()) + " bytes received");
    co_await c.async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(count(l));
    net::connection c = net::tcp::connect(l.local_endpoint());
    c.write("the whole request");
    c.close_write();  // the server's read_all ends here
    println("{}", c.read_all_text().value());
    server.wait();
    c.close();
}
```

Output:

```text
17 bytes received
```

## See also

- [close, async_close](close.md): both ways
- [read_all](read_all.md): what reads to the end the peer marks
- [sgcl::net::connection](../connection.md)
