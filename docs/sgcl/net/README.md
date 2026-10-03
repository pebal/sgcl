[sgcl](../README.md) › net

# sgcl::net

```cpp
#include "sgcl/net.h"   // namespace sgcl::net
```

What Go has in `net`, `net/netip` and `net/url`: IP addresses and networks as values, TCP and unix stream sockets,
UDP, the system's resolver, and URLs by the WHATWG URL Standard; HTTP/1.1 and HTTP/2 in [net::http](http/README.md)
(`#include "sgcl/net/http.h"`), TLS 1.3 in [net::tls](tls/README.md) (`#include "sgcl/net/tls.h"`, on
[crypto](../crypto/README.md)). The module depends on [core](../core/README.md), on [async](../async/README.md), whose
reactor, timers, blocking pool, `select` and stop tokens carry it, and on [io](../io/README.md), whose errors and
streams it uses: a [connection](connection.md) is a stream of io as it is, so `io::copy`, a
[buffered_reader](../io/buffered_reader.md) and every function over a reader or a writer take it.

The idea it rests on is that a connection is a handle and an address is a value. A [connection](connection.md), a
[listener](listener.md) and a [udp::socket](udp-socket.md) are handles of one word, a `tracked_ptr` to the object
inside, as a [string](../core/string.md) is: a copy is the same connection, as a `*net.TCPConn` is in Go, and a
handle passed into a task by value keeps the connection alive for as long as the task runs. An
[ip_address](ip_address.md) is thirty-two bytes, trivially copyable: nothing is allocated to parse, copy or compare
one. The protocols that make the handles — [tcp](tcp.md), [udp](udp.md), [unix_domain](unix_domain.md) — and the
resolver, [dns](dns.md), are classes of static functions: `net::tcp::connect(...)`, `net::dns::lookup(...)`.

Errors are values. Everything returns an `expected<T, io::error>` ([io::error](../io/error.md)): the value, or the
code, the operation and what it was on, so that `message()` reads
`read tcp 127.0.0.1:50000->127.0.0.1:8080: Operation timed out`. Nothing in the data path throws; an exception is a
broken contract only (a zone of more than fifteen bytes, a prefix length out of range).

## The rules

1. The names of the module are qualified, `net::tcp::connect`, `net::ip_address`: `net` is a namespace of its own, as
   every module but core has.
2. Every call that may wait for the network has two forms, as every call of io has
   ([The rules](../io/README.md#the-rules)): `c.read(b)` on a thread, which blocks it, or `co_await c.async_read(b)`
   in a task, which gives the worker back while it waits. [tcp::connect](tcp/connect.md) of a name resolves it and
   races the addresses on the scheduler, so on a worker it is only awaited; `connect` of an endpoint and
   [unix_domain::connect](unix_domain/connect.md) have a blocking form with no race and serve anywhere.
3. A handle holds a `tracked_ptr`, so it lives where one may: on a stack, in a task, in a managed object; in a global
   or a `std` container, a [rooted](../core/rooted.md) of it ([The rules](../core/README.md#the-rules), 1). A result
   reads `c->read(b)` on the `expected` and `c.read(b)` once unwrapped, never `(*c)->read(b)`.
4. The codes are `errno` values in the system category, the module's own in [category](category.md) ([errc](errc.md):
   `invalid_address`, `host_not_found`, `no_suitable_address`, and those of HTTP), the resolver's `EAI_*` in
   [lookup_category](lookup_category.md), and io's `errc::closed` for an operation on a connection the program
   closed. A deadline that passed is `ETIMEDOUT`, so `e.is_timeout()` answers; a stop token's stop is `ECANCELED`.
5. A connection's descriptor is closed by `close()` or, failing that, by the object's destructor on the collector's
   thread after the sweep that finds it dead. `close()` from another task is the way to cancel: the reads, writes and
   accepts in progress end with `io::errc::closed`. It is safe against the race of a close with an operation in
   progress: every operation holds the descriptor while it runs, and the `::close` is made by the last of them to let
   go, so an operation never lands on a number the kernel has given to another socket meanwhile.
6. A write to a connection the peer closed is an error, `EPIPE`, never a `SIGPIPE` (`SO_NOSIGPIPE`, `MSG_NOSIGNAL`).
   Every socket is close-on-exec.
7. `sgcl/net/url.h` alone brings [url](url.md) and [query_params](query_params.md) without the reactor or the
   sockets.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"net"` |
| [lookup_category](lookup_category.md) | `error.h` | the error category of the resolver's `EAI_*` codes |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |

## Classes

| Class | Header | Description |
|---|---|---|
| [connection](connection.md) | `connection.h` | a stream connection, TCP, unix, TLS or in memory: Go's `net.Conn` |
| [dns](dns.md) | `dns.h` | names to addresses and back, through the system's resolver |
| [endpoint](endpoint.md) | `ip.h` | an IP address and a port, `[::1]:443`: Go's `netip.AddrPort` |
| [ip_address](ip_address.md) | `ip.h` | an IPv4 or IPv6 address with its zone, a 32-byte value: RFC 4291 in, RFC 5952 out, the predicates |
| [ip_network](ip_network.md) | `ip.h` | an address and a prefix length, `10.0.0.0/8`: Go's `netip.Prefix` |
| [listener](listener.md) | `connection.h` | what accepts connections: Go's `net.Listener` |
| [query_params](query_params.md) | `url.h` | `application/x-www-form-urlencoded`: name and value pairs in their order |
| [tcp](tcp.md) | `socket.h` | TCP: connect with happy eyeballs, listen |
| [udp](udp.md) | `socket.h` | UDP: bind, connect |
| [udp::datagram](udp-datagram.md) | `socket.h` | what one receive of a UDP socket gives: the size, the sender, whether it was cut |
| [udp::socket](udp-socket.md) | `socket.h` | a UDP socket: Go's `net.UDPConn` |
| [unix_domain](unix_domain.md) | `socket.h` | unix stream sockets: connect, listen |
| [url](url.md) | `url.h` | a URL by the WHATWG URL Standard: parse, resolve, the parts, the setters as `with_*`, the origin |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the module's own error codes |

## Objects and types

| Name | Header | Description |
|---|---|---|
| `reuse_port` | `socket.h` | `inline constexpr reuse_port_t reuse_port{};`: the flag of a listener shared with other processes, `tcp::listen(":8080", net::reuse_port)` in each, `SO_REUSEPORT` ([tcp::listen](tcp/listen.md)) |
| `reuse_port_t` | `socket.h` | the type of `reuse_port`, a tag with an explicit default constructor |

## Namespaces

| Namespace | Header | Description |
|---|---|---|
| [http](http/README.md) | `http.h` (`http/http.h`) | HTTP/1.1 and HTTP/2: a client with a pool, a server with routes, the messages, headers, cookies, statuses |
| [tls](tls/README.md) | `tls.h`, `tls/error.h` | TLS 1.3 over the module's connections, both sides: connect, listen, the config, the identity, the state; the alerts and certificate failures as errors of the category `"tls"` |

## See also

- [Benchmarks](benchmarks.md): the single operations against Go
- [io](../io/README.md): the streams a connection is one of
- [async](../async/README.md): the reactor and the timers under the waits
- [The modules](../README.md)
