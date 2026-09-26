# sgcl::net

What Go has in `net`, `net/netip` and `net/url`: IP addresses and networks as values, TCP and unix stream sockets, UDP, the system's resolver, and URLs by the WHATWG URL Standard. `#include "sgcl/net/net.h"` brings the module in; it depends on [`core`](../core/README.md), [`containers`](../core/README.md), [`async`](../async/README.md) (the reactor, the timers, the blocking pool, `select` and the stop tokens carry it) and [`io`](../io/README.md) (its errors and streams); the index of the whole interface is [`docs/sgcl/`](../README.md). This is stages 1a to 1c of the module; HTTP/1.1 is [`net::http`](http/README.md) (`#include "sgcl/net/http/http.h"`), and TLS and HTTP/2 come after `crypto`. [`net::url`](url.md) needs neither the reactor nor sockets: `#include "sgcl/net/url.h"` alone brings only it (and [`txt`](../txt/README.md)'s IDNA and escaping).

## The names

The module is the namespace `sgcl::net`, as every module but core is: the protocols [`net::tcp`](socket.md), [`net::udp`](socket.md), [`net::unix_domain`](socket.md) and [`net::dns`](dns.md), the address types [`net::ip_address`](ip.md), [`net::ip_network`](ip.md) and [`net::endpoint`](ip.md), and the general types a connection is held by, [`net::connection`](connection.md), `net::listener`, `net::udp::socket`, `net::udp::datagram`, with the codes of [`net::errc`](error.md). After `using namespace sgcl::net;` a program writes `tcp::connect(...)`.

## Waiting

Every call that may wait for the network has two forms, as every call of io has ([stream](../io/stream.md)): `c.read(b)` on a thread, which blocks it, or `co_await c.async_read(b)` in a task, which gives the worker back while it waits. `net::tcp::connect(name)` resolves the name and races the addresses on the scheduler, so on a worker it is only awaited; `connect(endpoint)` and `net::unix_domain::connect` have a blocking form with no race and serve anywhere.

## Handles

A [`net::connection`](connection.md), a `net::listener` and a `net::udp::socket` are handles of one word, a `tracked_ptr` to the object inside, as a [`string`](../core/string.md) is: a copy is the same connection (as a `*net.TCPConn` in Go), a handle passed into a task by value keeps the connection alive for as long as the task runs, and a result reads `c->read(b)` on the `expected` and `c.read(b)` once unwrapped, never `(*c)->read(b)`. The addresses are plain values: an [`ip_address`](ip.md) is thirty-two bytes, trivially copyable, nothing allocated to parse, copy or compare one.

## Errors

Everything returns an [`expected<T, io::error>`](../io/error.md): the value, or an `io::error` with the code, the operation and what it was on, so that `message()` reads `dial tcp 127.0.0.1:1: Connection refused` or `read tcp 127.0.0.1:50000->127.0.0.1:8080: Operation timed out`. The codes are `errno` values in the system category, the module's own in [`net::category()`](error.md) (`invalid_address`, `host_not_found`, `no_suitable_address`), the resolver's `EAI_*` in `net::lookup_category()`, and io's `errc::closed` for an operation on a connection closed by the program. A deadline that passed is `ETIMEDOUT`, so `e.is_timeout()` answers; a stop token's stop is `ECANCELED`. Nothing in the data path throws; an exception is a broken contract only (a zone of more than fifteen bytes, a prefix length out of range).

## Lifetime and closing

A connection's descriptor is closed by `close()` or, failing that, by the object's destructor on the collector's thread after the sweep that finds it dead. `close()` from another task is the way to cancel: the reads, writes and accepts in progress end with `io::errc::closed`. It is safe against the race of a close with an operation in progress: every operation holds the descriptor while it runs, and the `::close` is made by the last of them to let go, so an operation never lands on a number the kernel has given to another socket meanwhile (`io/detail/descriptor.h`). A write to a connection the peer closed is an error, `EPIPE`, never a `SIGPIPE` (`SO_NOSIGPIPE`, `MSG_NOSIGNAL`). Every socket is close-on-exec.

## Pages

| page | header | what it is |
|---|---|---|
| [error](error.md) | `sgcl/net/error.h` | `net::errc` (the module's own codes), `net::category()`, `net::lookup_category()` (the `EAI_*` codes) |
| [ip](ip.md) | `sgcl/net/ip.h` | `ip_address` (IPv4, IPv6, the zone; RFC 4291 in, RFC 5952 out; the predicates), `ip_network` (CIDR), `net::endpoint` (address and port) |
| [connection](connection.md) | `sgcl/net/connection.h` | `net::connection` (read, write, lines, copy, half-close, deadlines, the io view, `in_memory`), `net::listener`, `net::udp::socket`, `net::udp::datagram` |
| [socket](socket.md) | `sgcl/net/socket.h` | `tcp` (connect with happy eyeballs, listen), `udp` (bind, connect), `unix_domain` (connect, listen), `net::reuse_port` |
| [url](url.md) | `sgcl/net/url.h` | `net::url` (the WHATWG URL Standard: parse, resolve, the parts, the setters as `with_*`, the origin), `net::query_params` (`application/x-www-form-urlencoded`) |
| [http](http/README.md) | `sgcl/net/http/http.h` | HTTP/1.1: `net::http::client` (a pool, redirects), `net::http::server` (Go 1.22 routes, limits, shutdown), the messages, headers, cookies, statuses |
| [dns](dns.md) | `sgcl/net/dns.h` | `net::dns::lookup`, `net::dns::reverse_lookup` and their `async_` forms, on the system's resolver |

## SGCL and Go

| Go | sgcl | note |
|---|---|---|
| `netip.Addr`, `ParseAddr`, `AddrFrom4`, `AddrFrom16` | `ip_address`, `net::ip_address::parse`, `net::ip_address::v4`, `net::ip_address::v6` | 32 bytes against 24: the zone lives in the value (at most 15 bytes), not behind a pointer to an interned string |
| `Addr.String`, `Is4`, `Is6`, `Is4In6`, `Unmap`, `Next`, `Prev`, `Zone`, `WithZone` | `to_string`, `is_v4`, `is_v6`, `is_v4_mapped`, `unmap`, `next`, `prev`, `zone`, `with_zone` | the text of RFC 5952, byte for byte Go's |
| `IsLoopback`, `IsPrivate`, `IsUnspecified`, `IsMulticast`, `IsLinkLocalUnicast`, `IsGlobalUnicast` | `is_loopback`, `is_private`, `is_unspecified`, `is_multicast`, `is_link_local`, `is_global_unicast` | `::%en0` is unspecified here (Go compares the zone too) |
| `netip.AddrPort`, `ParseAddrPort`, `netip.Prefix`, `ParsePrefix`, `Masked`, `Contains`, `Overlaps` | `net::endpoint`, `endpoint::parse`, `ip_network`, `net::ip_network::parse`, `masked`, `contains`, `overlaps` | `net::ip_network(a, 33)` throws; Go's `PrefixFrom` returns an invalid prefix |
| `net.Dial("tcp", a)`, `DialTimeout`, `DialContext` | `net::tcp::connect(a)`, `net::tcp::connect(a, 5s)`, `net::tcp::connect(a, token)`, and the `async_` forms | happy eyeballs as RFC 8305 has it: the families in turns, 250 ms apart |
| `net.Listen("tcp", a)`, `Accept`, `Close` | `net::tcp::listen(a)`, `l.accept()`, `l.accept()`, `l.close()` | a pause from 5 ms to 1 s when the descriptors run out, as Go's server; `net::reuse_port` for `SO_REUSEPORT` |
| `net.Conn`, `Read`, `Write`, `Close`, `CloseWrite`, `LocalAddr`, `RemoteAddr` | `net::connection`, `read`, `write`, `close`, `close_write`, `local_endpoint`, `remote_endpoint` | a handle of one word; each wait with its `async_` form |
| `SetDeadline`, `SetReadDeadline`, `SetWriteDeadline` | `set_deadline`, `set_read_deadline`, `set_write_deadline` | on the module's clock, so a test moves them with `manual_clock` |
| `SetNoDelay`, `SetKeepAlive`, `SetKeepAlivePeriod` | `set_no_delay`, `set_keep_alive(idle)` | the defaults are Go's: no Nagle, probes after 15 s |
| `io.ReadFull`, `io.ReadAll`, `io.Copy(c, c)` | `c.read_full(b)`, `c.read_all()`, `c.copy_to(c)` | |
| `bufio.NewReader(c).ReadString('\n')` | `c.read_line()` | an 8 KB buffer at the first line; lines bounded to 64 KB by default |
| a `net.Conn` as an `io.ReadWriteCloser` | `c` | the connection is a stream as it is: `buffered_reader`, `io::copy` take it |
| `net.Pipe` | `net::connection::in_memory()` | with deadlines and `close_write` |
| `net.ListenPacket("udp")`, `ReadFrom`, `WriteTo`, `net.Dial("udp")` | `net::udp::bind`, `receive_from`, `send_to`, `net::udp::connect`, `receive`, `send` | `datagram::truncated` says a datagram was cut to the buffer |
| `net.Dial("unix", p)`, `net.Listen("unix", p)` | `net::unix_domain::connect(p)`, `net::unix_domain::listen(p)` | the listener's close removes its file, as in Go |
| `net.LookupHost`, `LookupIP`, `LookupAddr` | `net::dns::lookup(h)`, `net::dns::reverse_lookup(ip)` | the system's resolver (`getaddrinfo`), its async form on the blocking pool; MX, SRV, TXT later |
| `net.ErrClosed`, `os.ErrDeadlineExceeded`, `*net.DNSError` | `io::errc::closed`, `ETIMEDOUT` (`is_timeout()`), `net::errc::host_not_found` and the `EAI_*` codes | one `io::error` for all |
| `ctx` of a `DialContext` or a lookup | a `stop_token` | `ECANCELED` when it stops |
| `url.Parse`, `URL.ResolveReference`, `URL.String` | `net::url::parse(s)`, `u.resolve(ref)` or `net::url::parse(ref, base)`, `u.to_string()` | the WHATWG URL Standard, not RFC 3986: IDNA hosts, `\` as `/`, the path resolved, `0x7f.1` an address; the whole of its test data passes |
| `URL.Scheme`, `User`, `Host`, `Hostname()`, `Port()`, `EscapedPath()`, `RawQuery`, `Fragment` | `scheme()`, `username()`/`password()`, `host()` and `port()`, `hostname()`, `port()`, `path()`, `query()`, `fragment()` | `host()` without the port; the parts escaped, as the URL writes them |
| setting a field of `url.URL` | `with_scheme`, `with_host`, `with_port`, `with_path`, `with_query`, `with_fragment`, `with_username`, `with_password` | the standard's setters, each a new `url`; `nullopt` where the standard refuses, so a `url` is always a URL |
| `url.Values`, `ParseQuery`, `Values.Encode`, `Get`, `Add`, `Set`, `Del` | `net::query_params`, `parse`, `to_string`, `get`, `add`, `set`, `erase` | a list in the order given (Go's is a map and `Encode` sorts); `parse` never fails, a space is `+` |
