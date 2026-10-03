[sgcl](../README.md) › [net](README.md)

# sgcl::net::listener

```cpp
#include "sgcl/net/connection.h"   // or "sgcl/net.h"

namespace sgcl::net {
    class listener;
}
```

`sgcl::net::listener` is a listening socket and the [connections](connection.md) it accepts: what
[tcp::listen](tcp/listen.md), [unix_domain::listen](unix_domain/listen.md) and [tls::listen](tls/listen.md) give.
It is Go's `net.Listener`: `Accept`, `Close` and `Addr` are [accept](listener/accept.md),
[close](listener/close.md) and [local_endpoint](listener/local_endpoint.md). A server is a loop of `accept` and a
task per connection; [net::http::server](http/server.md) takes a listener as it is, a TLS one included.

A listener is a handle of one word, a `tracked_ptr` to the object inside, as a [connection](connection.md) is: a
copy is the same listener, and a handle passed by value into a task keeps it alive for as long as the task runs.

## Rules

- A listener made by the default constructor holds none (`!l`); an operation on it is a contract violation (debug
  builds assert).
- A handle is a tracked word: on a stack, in a task, in a managed object; in a global or a `std` container, a
  [rooted](../core/rooted.md) of it.
- [accept](listener/accept.md) has no deadline of its own: [close](listener/close.md) from another task is how a
  wait for the next connection is ended (Go's `SetDeadline` on a listener, which a server uses for the same, is not
  here). The accepts in progress then end with `io::errc::closed`.
- When the descriptors run out, `accept` waits and tries again rather than spin or fail, as Go's server does.
- An accepted TCP connection has Nagle's algorithm off and keep-alive probes after 15 s, as a dialed one, as in Go.
- A unix listener's close removes its socket's file, as in Go.
- A listener not closed is closed by its destructor, on the collector's thread after the sweep that finds it dead.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](listener/listener.md) | constructs the handle: no listener, or a copy that is the same listener |
| `(destructor)` | releases the handle; the socket is closed by `close`, or when the collector finds the listener dead |
| `operator=` | makes the handle the same listener as another |

#### Accepting

| Function | Description |
|---|---|
| [accept, async_accept](listener/accept.md) | waits for the next connection |
| [close](listener/close.md) | stops listening, and ends the accepts in progress |
| [is_closed](listener/is_closed.md) | checks whether the listener was closed |

#### Observers

| Function | Description |
|---|---|
| [local_endpoint](listener/local_endpoint.md) | the address it listens on, the port the system chose included |
| [path](listener/path.md) | the path of a unix listener |
| [operator bool](listener/operator_bool.md) | checks whether the handle holds a listener |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](listener/operator_cmp.md) | checks whether two handles are the same listener |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> greet(net::connection c) {
    co_await c.async_write("hello\n");
    co_await c.async_close();
}

// Accepts until the listener is closed, a task per connection
async::task<> serve(net::listener l) {
    while (auto c = co_await l.async_accept()) {
        async::go(greet(*c));
    }
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(serve(l));
    for (int i : range(3)) {
        net::connection c = net::tcp::connect(l.local_endpoint());
        println("{}: {}", i, c.read_line()->value());
        c.close();
    }
    l.close();  // the accept in progress ends, and so does the loop
    server.wait();
}
```

Output:

```text
0: hello
1: hello
2: hello
```

## See also

- [tcp::listen](tcp/listen.md), [unix_domain::listen](unix_domain/listen.md): what makes a listener
- [connection](connection.md): what it accepts
- `tests/net/socket.cpp`
