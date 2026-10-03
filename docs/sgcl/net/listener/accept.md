[sgcl](../../README.md) › [net](../README.md) › [listener](../listener.md)

# sgcl::net::listener::accept, async_accept

```cpp
expected<connection, io::error> accept() const;                                // (1)
async::task<expected<connection, io::error>> async_accept() const noexcept;    // (2)
```

Waits for the next connection and returns it: Go's `Listener.Accept`. A connection aborted before it was taken
(`ECONNABORTED`) is skipped. When the descriptors run out (`EMFILE`, `ENFILE`, or the kernel's memory) the accept
waits, 5 ms and doubling to a second, and tries again rather than spin or fail, as Go's server does; macOS drops the
connection it could not take, Linux leaves it in the backlog.

There is no deadline: [close](close.md) from another task or thread ends a wait for the next connection. An
accepted TCP connection has Nagle's algorithm off and keep-alive probes after 15 s, as a dialed one; an IPv4 peer
of a listener of both families has its IPv4 address as its [remote_endpoint](../connection/remote_endpoint.md).

1. On the calling thread, which waits on the [reactor](../../async/readable.md) meanwhile.
2. The same for a task, which holds no worker while it waits.

## Parameters

None.

## Return value

The connection; or the [io::error](../../io/error.md), its operation `accept` and its path the listener (`tcp
127.0.0.1:8080`, `unix app.sock`): `io::errc::closed` after [close](close.md), the `errno` of an error that will not
pass otherwise.

## Complexity

One system call per connection, and one more per wait for readiness.

## Exceptions

- (1) `std::system_error` when the accept has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> serve(net::listener l) {
    for (;;) {
        auto c = co_await l.async_accept();
        if (!c) {
            println("{}", c.error().is_closed());
            co_return;
        }
        co_await c->async_write("welcome\n");
        c->close();
    }
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(serve(l));
    net::connection c = net::tcp::connect(l.local_endpoint());
    println("{}", c.read_line()->value());
    c.close();
    l.close();
    server.wait();
}
```

Output:

```text
welcome
true
```

## See also

- [close](close.md): ends the accepts in progress
- [tcp::connect](../tcp/connect.md): the other side
- [sgcl::net::listener](../listener.md)
