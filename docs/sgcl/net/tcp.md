[sgcl](../README.md) › [net](README.md)

# sgcl::net::tcp

```cpp
#include "sgcl/net/socket.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct tcp;
}
```

`sgcl::net::tcp` is TCP: connections out, [connect](tcp/connect.md), and in, [listen](tcp/listen.md), each with its
blocking form and its `async_` one. It is a structure of static functions, as [udp](udp.md) and
[unix_domain](unix_domain.md) are: `net::tcp::connect(a)` is Go's `net.Dial("tcp", a)`, with a timeout its
`DialTimeout`, with a stop token its `DialContext`; `net::tcp::listen(a)` is `net.Listen("tcp", a)`. What they
give is a [connection](connection.md) and a [listener](listener.md).

An address is `"host:port"`: a name (`"example.com:80"`), an IPv4 address (`"10.0.0.1:80"`), an IPv6 address in
brackets (`"[::1]:80"`), or no host (`":80"`), which is this machine to connect to and every address of both
families to listen on. The port is a number; service names (`"http"`) are not looked up. An
[endpoint](endpoint.md) dials one address as it is, with no lookup.

## Rules

- `connect` by name looks the name up ([dns](dns.md)) and races the addresses as RFC 8305 has it (happy eyeballs):
  the families in turns, an attempt 250 ms after the last or at once when one fails, the first connection the one
  returned. A timeout bounds the whole of it, the lookup included; a stop token ends it.
- The blocking forms of a `connect` by name run the race on the scheduler and wait for it, so they are for a thread,
  as [task::wait](../async/task/wait.md) is (debug builds assert); a task awaits `async_connect`. `connect(endpoint)` and
  [unix_domain::connect](unix_domain/connect.md) dial one address with no race: they block the calling thread
  wherever it is, a worker included, as a blocking read of `io::file` does, and a worker so blocked runs no other
  task meanwhile, so a task awaits their `async_` forms.
- `listen` binds with `SO_REUSEADDR` and listens with the system's backlog. No host is the IPv6 wildcard with
  `IPV6_V6ONLY` off, one socket for both families as in Go (an IPv4 socket on a system without IPv6); a name
  listens on its first IPv4 address, or its first. `net::reuse_port` lets other processes listen on the same port,
  the kernel spreading the connections.
- A new connection, dialed or accepted, has Nagle's algorithm off and keep-alive probes after 15 s of silence, as in
  Go; every socket is non-blocking, close-on-exec and free of `SIGPIPE`.
- Errors are values, [expected\<T, io::error\>](../io/error.md), the operation named as Go names it: `dial tcp`,
  `listen tcp`, `lookup`.

## Member functions

| Function | Description |
|---|---|
| [connect, async_connect](tcp/connect.md) | connects to an address, racing its addresses (static) |
| [listen, async_listen](tcp/listen.md) | listens on an address (static) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<> echo(net::connection c) {
    co_await c.async_copy_to(c);  // reads to the end and writes it back
    co_await c.async_close();
}

async::task<> serve(net::listener l) {
    while (auto c = co_await l.async_accept()) {
        async::go(echo(*c));  // a task per connection
    }
}

async::task<> ask(string address) {
    auto c = co_await net::tcp::async_connect(address, 5s);
    if (!c) {
        println("{}", c.error().message());
        co_return;
    }
    co_await c->async_write("ping");
    c->close_write();
    println("{}", (co_await c->async_read_all_text()).value());
    co_await c->async_close();
}

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto server = async::spawn(serve(l));
    async::run(ask(l.local_endpoint().to_string()));
    l.close();
    server.wait();
}
```

Output:

```text
ping
```

## See also

- [connection](connection.md), [listener](listener.md): what it makes
- [dns](dns.md): the lookup of a name; [endpoint](endpoint.md): an address and a port
- [udp](udp.md), [unix_domain](unix_domain.md): the other protocols
- RFC 8305 §4–5 (happy eyeballs); `tests/net/dial.cpp` runs the race step by step on the manual clock
- `tests/net/socket.cpp`: echo from 1 B to 16 MB, half-close, dual stack, `reuse_port`
