[sgcl](../README.md) › net

# sgcl::net

```cpp
#include "sgcl/net.h"   // namespace sgcl::net
```

What Go has in `net`, `net/netip` and `net/url`: IP addresses and networks as values, TCP and unix stream sockets,
UDP with multicast, the machine's interfaces, names (the system's resolver for addresses, a stub resolver of the module's own for MX, TXT, SRV, NS and CNAME, over UDP, TCP, TLS or HTTPS), multicast DNS and DNS-SD (the services of the link, browsed and published),
and URLs by the WHATWG URL Standard; HTTP/1.1 and HTTP/2 in [net::http](http/README.md)
(`#include "sgcl/net/http.h"`), IMAP in [net::imap](imap/README.md) (`#include "sgcl/net/imap.h"`), POP3 in [net::pop3](pop3/README.md) (`#include "sgcl/net/pop3.h"`), LDAP in [net::ldap](ldap/README.md) (`#include "sgcl/net/ldap.h"`), JSON-RPC 2.0 in [net::jsonrpc](jsonrpc/README.md) (`#include "sgcl/net/jsonrpc.h"`), AMQP 0-9-1 in [net::amqp](amqp/README.md) (`#include "sgcl/net/amqp.h"`), NATS in [net::nats](nats/README.md) (`#include "sgcl/net/nats.h"`), MQTT in [net::mqtt](mqtt/README.md) (`#include "sgcl/net/mqtt.h"`), SMTP in [net::smtp](smtp/README.md) (`#include "sgcl/net/smtp.h"`) with the checks of a message's origin, [net::spf](spf/README.md), [net::dkim](dkim/README.md) and [net::dmarc](dmarc/README.md), TLS 1.3 (and a client's TLS 1.2) in [net::tls](tls/README.md) (`#include "sgcl/net/tls.h"`, on
[crypto](../crypto/README.md)), SSH's client and server in [net::ssh](ssh/README.md) (`#include "sgcl/net/ssh.h"`), SFTP over it in [net::sftp](sftp/README.md) (`#include "sgcl/net/sftp.h"`), certificates from an ACME CA in [net::acme](acme/README.md) (`#include "sgcl/net/acme.h"`), WebDAV in [net::webdav](webdav/README.md) (`#include "sgcl/net/webdav.h"`), the time of a server in [net::ntp](ntp/README.md) (`#include "sgcl/net/ntp.h"`), an ICMP echo in [ping](ping.md) (`#include "sgcl/net/ping.h"`), tokens of an OAuth 2.0 server in [net::oauth2](oauth2/README.md) (`#include "sgcl/net/oauth2.h"`), and sign-in by OpenID Connect in [net::oidc](oidc/README.md) (`#include "sgcl/net/oidc.h"`). The module depends on [core](../core/README.md), on [async](../async/README.md), whose
reactor, timers, blocking pool, `select` and stop tokens carry it, and on [io](../io/README.md), whose errors and
streams it uses: a [connection](connection/README.md) is a stream of io as it is, so `io::copy`, a
[buffered_reader](../io/buffered_reader/README.md) and every function over a reader or a writer take it.

The idea it rests on is that a connection is a handle and an address is a value. A [connection](connection/README.md), a
[listener](listener/README.md) and a [udp::socket](udp-socket/README.md) are handles of one word, a `tracked_ptr` to the object
inside, as a [string](../core/string/README.md) is: a copy is the same connection, as a `*net.TCPConn` is in Go, and a
handle passed into a task by value keeps the connection alive for as long as the task runs. An
[ip_address](ip_address/README.md) is thirty-two bytes, trivially copyable: nothing is allocated to parse, copy or compare
one. The protocols that make the handles — [tcp](tcp/README.md), [udp](udp/README.md), [unix_domain](unix_domain/README.md) — and the
resolver, [dns](dns/README.md), are classes of static functions: `net::tcp::connect(...)`, `net::dns::lookup(...)`;
so is [socks5](socks5/README.md), a connection through a SOCKS5 proxy, `net::socks5::connect(proxy, target)`, and the proxy
itself, [socks5::server](socks5-server/README.md).

Errors are values. Everything returns an `expected<T, io::error>` ([io::error](../io/error/README.md)): the value, or the
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
   or a `std` container, a [rooted](../core/rooted/README.md) of it ([The rules](../core/README.md#the-rules), 1). A result
   reads `c->read(b)` on the `expected` and `c.read(b)` once unwrapped, never `(*c)->read(b)`.
4. The codes are `errno` values in the system category, the module's own in [category](category.md) ([errc](errc.md):
   `invalid_address`, `host_not_found`, `no_suitable_address`, the resolver's `no_data`, `server_failure` and
   `server_misbehaving`, and those of HTTP, SSH and SMTP), the resolver's `EAI_*` in
   [lookup_category](lookup_category.md), and io's `errc::closed` for an operation on a connection the program
   closed. A deadline that passed is `ETIMEDOUT`, so `e.is_timeout()` answers; a stop token's stop is `ECANCELED`.
5. A connection's descriptor is closed by `close()` or, failing that, by the object's destructor on the collector's
   thread after the sweep that finds it dead. `close()` from another task is the way to cancel: the reads, writes and
   accepts in progress end with `io::errc::closed`. It is safe against the race of a close with an operation in
   progress: every operation holds the descriptor while it runs, and the `::close` is made by the last of them to let
   go, so an operation never lands on a number the kernel has given to another socket meanwhile.
6. A write to a connection the peer closed is an error, `EPIPE`, never a `SIGPIPE` (`SO_NOSIGPIPE`, `MSG_NOSIGNAL`).
   Every socket is close-on-exec.
7. `sgcl/net/url.h` alone brings [url](url/README.md) and [query_params](query_params/README.md) without the reactor or the
   sockets.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"net"` |
| [interface_by_index](interface_by_index.md) | `interface.h` | the network interface of an index: Go's `net.InterfaceByIndex` |
| [interface_by_name](interface_by_name.md) | `interface.h` | the network interface of a name: Go's `net.InterfaceByName` |
| [interfaces](interfaces.md) | `interface.h` | the network interfaces of this machine with their addresses: Go's `net.Interfaces` |
| [lookup_category](lookup_category.md) | `error.h` | the error category of the resolver's `EAI_*` codes |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |
| [ping, async_ping](ping.md) | `ping.h` | an ICMP echo (RFC 792, RFC 4443) and its reply: who answered, the round trip, over an unprivileged ICMP socket or a raw one |

## Classes

| Class | Header | Description |
|---|---|---|
| [connection](connection/README.md) | `connection.h` | a stream connection, TCP, unix, TLS or in memory: Go's `net.Conn` |
| [dns](dns/README.md) | `dns.h` | names to addresses and back through the system's resolver; MX, TXT, SRV, NS, CNAME, HTTPS and SVCB, and addresses given options, through the module's own (RFC 1035), over UDP, TCP, TLS (RFC 7858) or HTTPS (RFC 8484) |
| [dns::mx](dns-mx.md) | `dns.h` | a mail exchanger: its host and preference, Go's `net.MX` |
| [dns::options](dns-options.md) | `dns.h` | how a lookup goes through the module's resolver: the servers, the wait for one answer, the rounds, the profile of TLS |
| [dns::server](dns-server.md) | `dns.h` | a server to ask: its address with the transport, UDP, TCP, TLS or HTTPS, the name checked, the pins |
| [dns::srv](dns-srv.md) | `dns.h` | a server of a service: its target, port, priority and weight, Go's `net.SRV` |
| [dns::svc_param](dns-svc_param.md) | `dns.h` | a SvcParam of an HTTPS or SVCB record of a key dns::svcb has no field for |
| [dns::svcb](dns-svcb.md) | `dns.h` | a service binding (RFC 9460), an HTTPS or SVCB record: priority, target, ALPN, port, hints, ECH |
| [endpoint](endpoint/README.md) | `ip.h` | an IP address and a port, `[::1]:443`: Go's `netip.AddrPort` |
| [ip_address](ip_address/README.md) | `ip.h` | an IPv4 or IPv6 address with its zone, a 32-byte value: RFC 4291 in, RFC 5952 out, the predicates |
| [ip_network](ip_network/README.md) | `ip.h` | an address and a prefix length, `10.0.0.0/8`: Go's `netip.Prefix` |
| [listener](listener/README.md) | `connection.h` | what accepts connections: Go's `net.Listener` |
| [mdns](mdns/README.md) | `mdns.h` | multicast DNS (RFC 6762): the addresses of a host of the link, and the responder |
| [mdns::options](mdns-options.md) | `mdns.h` | the interfaces and families of multicast DNS, a lookup's wait, a responder's host name |
| [mdns::responder](mdns-responder/README.md) | `mdns.h` | a multicast DNS responder: this host's name and addresses, the services published on it |
| [network_interface](network_interface.md) | `interface.h` | a network interface: its name, index, addresses, flags; Go's `net.Interface` |
| [ping_options](ping_options.md) | `ping.h` | the timeout, the payload's size, the TTL and the stop of an echo |
| [ping_reply](ping_reply.md) | `ping.h` | an echo's reply: who answered, the round trip, the size, the TTL |
| [query_params](query_params/README.md) | `url.h` | `application/x-www-form-urlencoded`: name and value pairs in their order |
| [socks5](socks5/README.md) | `socks5.h` | SOCKS5 (RFC 1928, RFC 1929): a connection to a target through a proxy, the name resolved there |
| [socks5::options](socks5-options.md) | `socks5.h` | the credentials, the timeout and the stop of a connection through a SOCKS5 proxy |
| [socks5::server](socks5-server/README.md) | `socks5_server.h` (`socks5.h`) | a SOCKS5 proxy (RFC 1928, RFC 1929): CONNECT, BIND, UDP ASSOCIATE, username and password, the connections out made by the program's dial |
| [tcp](tcp/README.md) | `socket.h` | TCP: connect with happy eyeballs, listen |
| [udp](udp/README.md) | `socket.h` | UDP: bind, connect, listen to a multicast group |
| [udp::datagram](udp-datagram.md) | `socket.h` | what one receive of a UDP socket gives: the size, the sender, whether it was cut |
| [udp::socket](udp-socket/README.md) | `socket.h` | a UDP socket: Go's `net.UDPConn`, with its multicast memberships and settings |
| [unix_domain](unix_domain/README.md) | `socket.h` | unix stream sockets: connect, listen |
| [url](url/README.md) | `url.h` | a URL by the WHATWG URL Standard: parse, resolve, the parts, the setters as `with_*`, the origin |

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
| [acme](acme/README.md) | `acme.h` (`acme/acme.h`) | ACME (RFC 8555): a client of the whole protocol, a manager of certificates obtained at the first handshake and renewed by themselves for a TLS server, a test CA on the loopback |
| [amqp](amqp/README.md) | `amqp.h` (`amqp/amqp.h`) | AMQP 0-9-1 (RabbitMQ's protocol): a client of a broker's exchanges and queues, publisher confirms, consumers with prefetch, acknowledgements, field tables, heartbeats |
| [dkim](dkim/README.md) | `dkim.h` | DomainKeys Identified Mail (RFC 6376, RFC 8463): a message signed with a domain's key, its signatures verified against the keys in DNS |
| [dmarc](dmarc/README.md) | `dmarc.h` | DMARC (RFC 7489): a From domain's policy found, SPF's and DKIM's domains aligned with it, the disposition; aggregate reports read |
| [dns_sd](dns_sd/README.md) | `mdns.h` | DNS-based service discovery (RFC 6763): the services of the link browsed, resolved, listed and published |
| [http](http/README.md) | `http.h` (`http/http.h`) | HTTP/1.1 and HTTP/2: a client with a pool, a server with routes, the messages, headers, cookies, statuses |
| [imap](imap/README.md) | `imap.h` (`imap/imap.h`) | IMAP4rev2, both sides: a client of typed messages and searches, a server over a backend in memory or in Maildir directories; STARTTLS, IDLE, CONDSTORE and QRESYNC, MOVE, SORT, THREAD, COMPRESS |
| [jsonrpc](jsonrpc/README.md) | `jsonrpc.h` (`jsonrpc/jsonrpc.h`) | JSON-RPC 2.0 both ways: methods served over a stream (LSP's Content-Length framing, or lines), a WebSocket or HTTP; peers that call and serve at once, batches, notifications, cancellation |
| [ldap](ldap/README.md) | `ldap.h` (`ldap/ldap.h`) | LDAP (RFC 4511): a client that binds (simple, SASL PLAIN and EXTERNAL), searches with RFC 4515's filters and paged results, adds, modifies, renames, compares and removes entries; ldaps:// and StartTLS, operations at once on one connection |
| [mqtt](mqtt/README.md) | `mqtt.h` (`mqtt/mqtt.h`) | MQTT 5.0 and 3.1.1 both ways: a client over TCP, TLS or WebSocket (every QoS, sessions, wills, aliases, shared subscriptions), a broker with sessions and retained messages in memory |
| [nats](nats/README.md) | `nats.h` (`nats/nats.h`) | NATS: a client of a server's subjects with wildcards and queue groups, request and reply over one inbox, headers, nkeys and JWTs, TLS |
| [ntp](ntp/README.md) | `ntp.h` | SNTP (RFC 4330, RFC 5905): a time server asked once, this clock's offset and the round trip's delay, the server's stratum and reference |
| [oauth2](oauth2/README.md) | `oauth2.h` (`oauth2/oauth2.h`) | OAuth 2.0 from the client's side: the authorization code grant with PKCE, client credentials, the device flow, refresh, revocation, introspection; a token source that refreshes by itself and an http::client that sends its tokens |
| [oidc](oidc/README.md) | `oidc.h` (`oidc/oidc.h`) | OpenID Connect on oauth2: a provider found by its issuer, ID tokens verified with crypto::jose (signature, issuer, audience, expiry, nonce), userinfo |
| [pop3](pop3/README.md) | `pop3.h` (`pop3/pop3.h`) | POP3 both ways: a client of the maildrop (list, retrieve, top, remove; STLS, pop3s, AUTH PLAIN, APOP, PIPELINING), a server of the INBOX of an imap::backend |
| [sftp](sftp/README.md) | `sftp.h` (`sftp/sftp.h`) | SFTP version 3 both ways over [ssh](ssh/README.md): a client with remote files as io's streams, attributes, directories, renames and links, whole files in one line; a server of a directory, every path kept within it |
| [smtp](smtp/README.md) | `smtp.h` (`smtp/smtp.h`) | SMTP both ways on the messages of [encoding::email](../encoding/email/README.md): `send` in one line, a client session, delivery by MX, a server with a handler per message; STARTTLS, implicit TLS, AUTH, PIPELINING, 8BITMIME, SMTPUTF8, SIZE, DSN, CHUNKING |
| [spf](spf/README.md) | `spf.h` | the Sender Policy Framework (RFC 7208): whether a domain lets a host send its mail, every mechanism, macro and limit |
| [ssh](ssh/README.md) | `ssh.h` (`ssh/ssh.h`) | SSH, both sides: a client with sessions, commands and forwarding both ways, a server with authentication by callbacks and a handler of sessions, keys, known_hosts, authorized_keys, the agent |
| [tls](tls/README.md) | `tls.h`, `tls/error.h` | TLS 1.3 over the module's connections, both sides, and a client's TLS 1.2: connect, listen, the config, the identity, the state, client certificates, session resumption (the session cache, the ticket keys); the alerts and certificate failures as errors of the category `"tls"` |
| [webdav](webdav/README.md) | `webdav.h` (`webdav/webdav.h`) | WebDAV both ways (RFC 4918, classes 1 and 2): a server of a directory as an HTTP handler, every path kept within it, properties and locks; a client of a server's tree |

## See also

- [Benchmarks](benchmarks.md): the single operations against Go
- [io](../io/README.md): the streams a connection is one of
- [async](../async/README.md): the reactor and the timers under the waits
- [The modules](../README.md)
