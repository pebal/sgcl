# sgcl::net::tcp, sgcl::net::udp, sgcl::net::unix_domain

```cpp
#include "sgcl/net/socket.h"   // or "sgcl/net/net.h"

namespace sgcl::net {
    struct tcp;           // connect (happy eyeballs), listen
    struct udp;           // bind, connect
    struct unix_domain;   // connect, listen: stream sockets in the file system
    namespace net {
        struct reuse_port_t;
        inline constexpr reuse_port_t reuse_port;   // SO_REUSEPORT: tcp::listen(":8080", net::reuse_port)
    }
}
```

The protocols that make [connections](connection.md): structures of static functions, each with its blocking form and its `async_` one. An address is `"host:port"`: a name (`"example.com:80"`), an IPv4 address (`"10.0.0.1:80"`), an IPv6 address in brackets (`"[::1]:80"`), or no host (`":80"`), which is this machine to connect to and every address of both families to listen on. The port is a number; service names (`"http"`) are not looked up.

## Rules

- **connect** looks the name up ([dns](dns.md)) and races the addresses as RFC 8305 has it (happy eyeballs): the addresses of the two families taken in turns (§4), the first family the first address's; an attempt started, the next one 250 ms later (§5) or at once when one fails; the first connection the one returned, the other attempts stopped, a connection that comes after the winner's closed. When every attempt fails the error is the first attempt's, as in Go. A timeout (`connect(a, 5s)`) bounds the whole of it, the lookup included, with `ETIMEDOUT`; a stop token ends it with `ECANCELED`, and wins over a connection that comes at the same moment.
- **The blocking forms of a connect by name** run the race on the scheduler and wait for it, so they are for a thread, as `async::task::join` is (debug builds assert); a task awaits `connect`. `connect(endpoint)` and `net::unix_domain::connect` dial one address with no race: they block the calling thread wherever it is, a worker included, as a blocking read of `io::file` does (a worker so blocked runs no other task meanwhile, so a task awaits the `async_` form).
- **A timeout at the clock's end is none**: `connect(a, duration::max())` saturates at `time_point::max()`, where no timer fires.
- **listen** binds with `SO_REUSEADDR` and listens with the system's backlog. No host is the IPv6 wildcard with `IPV6_V6ONLY` off, one socket for both families as in Go (an IPv4 socket on a system without IPv6); a name listens on its first IPv4 address, or its first. `net::reuse_port` lets other processes listen on the same port, the kernel spreading the connections.
- **A new connection** has Nagle's algorithm off and keep-alive probes after 15 s of silence, as in Go; every socket is non-blocking, close-on-exec and free of `SIGPIPE`.
- **unix_domain::listen** creates the socket's file (an error when anything is at the path, as in Go) and the listener's `close()` removes it. A path is at most 103 bytes on macOS, 107 on Linux (`net::errc::invalid_address` past that).
- **udp** waits for nothing on the network: `bind` and `connect` differ from their `async_` forms only in the lookup of a name. `connect` fixes the peer, so that `send` and `receive` need no address and the system drops datagrams from anyone else.

## Members

### tcp

```cpp
static expected<net::connection, io::error> connect(const string& address);
static expected<net::connection, io::error> connect(const string& address, async::stop_token stop);
static expected<net::connection, io::error> connect(const string& address, duration timeout);
static expected<net::connection, io::error> connect(const net::endpoint& to);                 // no lookup, no race
static async::task<expected<net::connection, io::error>> async_connect(const string& address);
static async::task<expected<net::connection, io::error>> async_connect(const string& address, async::stop_token stop);
static async::task<expected<net::connection, io::error>> async_connect(const string& address, duration timeout);
static async::task<expected<net::connection, io::error>> async_connect(const net::endpoint& to);
static expected<net::listener, io::error> listen(const string& address);
static expected<net::listener, io::error> listen(const string& address, net::reuse_port_t);
static async::task<expected<net::listener, io::error>> async_listen(const string& address);
static async::task<expected<net::listener, io::error>> async_listen(const string& address, net::reuse_port_t);
```

```cpp
async::task<> echo(net::connection c) {
    co_await c.async_copy_to(c);        // reads to the end and writes it back
    co_await c.async_close();
}

async::task<> serve() {
    auto listener = co_await net::tcp::async_listen(":8080");
    if (!listener) {
        std::cerr << listener.error().message() << '\n';
        co_return;
    }
    while (auto c = co_await listener->async_accept()) {
        async::go(echo(*c));                   // a task per connection
    }
}

int main() {
    serve().wait();
}
```

### udp

```cpp
static expected<net::udp::socket, io::error> bind(const string& address);                // ":5353": every address of both families
static async::task<expected<net::udp::socket, io::error>> async_bind(const string& address);
static expected<net::udp::socket, io::error> connect(const string& address);             // the peer fixed; no host is 127.0.0.1
static async::task<expected<net::udp::socket, io::error>> async_connect(const string& address);
```

### unix_domain

```cpp
static expected<net::connection, io::error> connect(const string& path);
static async::task<expected<net::connection, io::error>> async_connect(const string& path);
static expected<net::listener, io::error> listen(const string& path);                   // close() removes the file
static async::task<expected<net::listener, io::error>> async_listen(const string& path);
```

```cpp
auto l = net::unix_domain::listen("/tmp/app.sock");
auto c = net::unix_domain::connect("/tmp/app.sock");
```

## See also

- [connection](connection.md): what they make; [dns](dns.md): the lookup of a name; [ip](ip.md): `net::endpoint`
- RFC 8305 §4–5 (happy eyeballs); `tests/net/dial.cpp` runs the race step by step on the manual clock
- `tests/net/socket.cpp`: echo from 1 B to 16 MB, half-close, dual stack, `reuse_port`, a unix socket's file, UDP
