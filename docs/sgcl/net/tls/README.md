[sgcl](../../README.md) › [net](../README.md) › tls

# sgcl::net::tls

```cpp
#include "sgcl/net/tls.h"   // namespace sgcl::net::tls
```

TLS 1.3 ([RFC 8446](https://www.rfc-editor.org/rfc/rfc8446)) over the module's [connections](../connection.md), both
sides, as Go's `crypto/tls` gives it. [connect](connect.md) dials TCP, completes the handshake and only then gives the
connection out: a [net::connection](../connection.md) like any other, with `read`, `read_line`, `write`, deadlines,
`close` and the `async_` forms, so that [http](../http/README.md) and every stream of [io](../../io/README.md)
take it as they take a TCP one. The server's chain is verified by [crypto::x509](../../crypto/x509.md) against the
system's roots or a pool of the program's, for the name dialed. [listen](listen.md) is a
[net::listener](../listener.md) whose `accept` gives connections whose handshake is done, with the certificate and
the key of an [identity](identity.md); [http::server](../http/server.md) serves it as it serves a TCP one
(https). [client](client.md) and [server](server.md) are the two handshakes over a connection the program already
has.

Only TLS 1.3: the groups X25519MLKEM768 (the post-quantum hybrid, first by default), X25519, P-256 and P-384
([group](group.md)); the cipher suites AES-128-GCM, ChaCha20-Poly1305 and AES-256-GCM ([cipher](cipher.md));
certificates of Ed25519, ECDSA P-256 and P-384, and RSA (PSS). Interoperability was tested against OpenSSL
(`s_server`, `s_client`) and Go's `crypto/tls`, each as client and as server: every cipher suite × every group ×
three kinds of certificate on each side, HelloRetryRequest, ALPN, KeyUpdate and `close_notify`. **The
implementation has not been through an independent cryptographic audit.**

The settings of a connection are one value, a [config](config.md), Go's `tls.Config`; what the handshake settled
is another, a [state](state.md), read back by [state_of](state_of.md). A failure is an
[io::error](../../io/error.md) like every other of the module, in a category of its own, `"tls"`
([category](category.md)), which [alert_of](alert_of.md), [is_remote](is_remote.md) and
[certificate_reason](certificate_reason.md) take apart.

## The rules

1. The handshake comes before the connection. [connect](connect.md) and [client](client.md) return once the
   handshake is complete and the server's chain verified, [server](server.md) once the client's `Finished` is
   verified, and a listener's `accept` gives only connections whose handshake is done. A failure is an `io::error`
   and no connection. A connection the program handed to `client` or `server` is closed when its handshake fails.
2. A timeout bounds the whole of it. [config::handshake_timeout](config.md) (10 s) covers the TCP connect and the
   handshake together, the lookup of the name included, and ends them with `ETIMEDOUT` (`is_timeout()`). While the
   handshake runs it sets the transport's deadlines, and it removes them afterwards: deadlines set on the TLS
   connection later are the connection's own, as for TCP.
3. Full duplex, as every connection. One read and one write may run at once, from two tasks or two threads. They
   share only the keys of the writing direction, taken for one record at a time, so the two interleave by whole
   records. A read that must answer something (a `KeyUpdate` that asks for one, a fatal alert) takes the writing
   keys for that one record. A write is cut into records of at most 16 KB, or of the server's `record_size_limit`
   ([RFC 8449](https://www.rfc-editor.org/rfc/rfc8449)). The keys of a direction are updated (RFC 8446 §5.5)
   before they reach their limit.
4. `close()` does not wait; `async_close()` waits at most 100 ms. Both send `close_notify`, as Go's `Conn.Close`
   does, and then close the transport. `close()` sends it only when no write holds the record and the socket takes
   it at once; otherwise it closes without it, as a close from another task has to, since it is how a stuck read
   or write is cancelled. `async_close()` waits for the record a write holds and for `close_notify` to go out,
   100 ms for the two together. `close_write()`, Go's `Conn.CloseWrite`, sends `close_notify` after the write in
   progress, then ends the transport's writing half: the peer reads the end, and this side still reads. A write
   after `close_notify` is `io::errc::closed`.
5. The end of the stream is the peer's `close_notify`: `read` returns 0 and `read_line` returns `nullopt`. The
   transport's end at a record's boundary without `close_notify` is also taken as the end, as the TCP connection's
   end is: a protocol whose messages carry their own lengths (HTTP) sees a cut message as such. The transport's end
   in the middle of a record is `io::errc::unexpected_eof`.
6. What the server chooses, from the client's first ClientHello: the first cipher suite of its list the client
   offers; the first group of its list the client supports and sent a share of, else the first of its list the
   client supports, asked for with a HelloRetryRequest (Go's rule); the first identity whose certificate is for the
   name the client sent (SNI), else the first, and of it the first signature scheme of its key the client offers;
   the first ALPN protocol of its list the client offers (a client that offers some and none of them is refused with
   `no_application_protocol`; a server with no list answers none). No common suite, group or scheme is
   `handshake_failure`.
7. The defaults are Go's and the browsers'. Groups in the order X25519MLKEM768, X25519, P-256, P-384. The first
   ClientHello carries a key share of the hybrid and one of X25519, so a server of either answers in one round
   trip, and a server of P-256 or P-384 alone costs a HelloRetryRequest. Cipher suites in the order AES-128-GCM,
   ChaCha20-Poly1305, AES-256-GCM. A [config](config.md) narrows both lists and reorders them; the first group given
   is the one shared first.
8. The middlebox compatibility mode is on (RFC 8446 §D.4): a session id of 32 random bytes and a
   `change_cipher_spec` record after the first flight, as Go, OpenSSL and the browsers send them. It is not a
   setting.
9. The server's name is `config::server_name`, or the host of the address given to [connect](connect.md). It is
   sent as SNI (never for an IP address) and checked against the leaf's names, or its IP addresses for an address.
   `insecure_skip_verify` takes the chain unchecked. It is for tests: with it, any machine in the path can read and
   change the traffic.
10. A private key never lies in managed memory. An [identity](identity.md) keeps its key in an unmanaged block of
    its own, never copied, and zeroed when the identity is collected; its PEM is read where it lies, from a
    [secret_bytes](../../crypto/secret_bytes.md) of [crypto::read_secret](../../crypto/secret.md). It is the
    server's: the client of v1 sends no certificate.
11. The values of the namespace hold tracked pointers: an [identity](identity.md) is a handle of one word, and a
    [config](config.md) and a [state](state.md) hold vectors, strings and certificates. They live where a
    `tracked_ptr` may: on a stack, in a task, in a managed object; in a global, under a
    [rooted](../../core/rooted.md).
12. Not in v1: client certificates (mTLS, Go's `ClientAuth` and `GetClientCertificate`: a server's
    `CertificateRequest` is answered with an empty `Certificate`, and the server decides; the server of v1 asks for
    none), session resumption and PSK (Go's `ClientSessionCache` and session tickets: a client reads a
    `NewSessionTicket` and passes it over; the server sends none), 0-RTT early data (the server refuses it and skips
    the client's early records, RFC 8446 §4.2.10), and TLS 1.2 or older (a peer without TLS 1.3 fails with
    `protocol_version`).
13. Errors are `io::error`s in the category `"tls"`, worded as Go words them. An alert this side sent reads
    `tls: bad certificate`; the code is the alert's number, so `e.code() == net::tls::alert::decode_error` asks
    for it ([alert](alert.md)). An alert the peer sent reads `remote error: tls: handshake failure`; one a server
    sends before its hello, in the clear, reads `remote error: tls: no application protocol, before the server's
    hello`, and a `close_notify` or a `protocol_version` there — how a server without TLS 1.3 answers a hello of 1.3
    alone — adds `(no TLS 1.3?)`; `alert_of` and `is_remote` give the alert. A chain that did not verify reads
    `tls: certificate signed by unknown authority`, and [certificate_reason](certificate_reason.md) gives the
    [x509::reason](../../crypto/x509-reason.md); the client's alert for it goes under the handshake keys, so the
    server reads `remote error: tls: bad certificate` (or `unknown certificate authority`). A
    [config](config.md) the handshake cannot
    start with is `EINVAL`, before a record is sent. The transport's own failures (`ETIMEDOUT`, `ECONNRESET`) stay
    what they are.

## Functions

| Function | Header | Description |
|---|---|---|
| [alert_of](alert_of.md) | `tls/error.h` | the alert of a TLS error, this side's or the peer's |
| [category](category.md) | `tls/error.h` | the error category of TLS, `"tls"` |
| [certificate_reason](certificate_reason.md) | `tls/error.h` | why the server's chain did not verify |
| [client, async_client](client.md) | `tls.h` | the client's handshake over a connection there is |
| [connect, async_connect](connect.md) | `tls.h` | a TCP connection to an address and the client's handshake over it |
| [is_remote](is_remote.md) | `tls/error.h` | checks whether the peer sent the alert of an error |
| [listen, async_listen](listen.md) | `tls.h` | a TCP listener whose `accept` gives connections after their handshake |
| [make_error_code](make_error_code.md) | `tls/error.h` | an alert as a `std::error_code` |
| [server, async_server](server.md) | `tls.h` | the server's handshake over a connection there is |
| [state_of](state_of.md) | `tls.h` | what the handshake of a connection settled |

## Classes

| Class | Header | Description |
|---|---|---|
| [config](config.md) | `tls.h` | the settings of a connection, a value: Go's `tls.Config` |
| [identity](identity.md) | `tls.h` | a server's certificate chain with its private key, a handle of one word |
| [state](state.md) | `tls.h` | what the handshake settled: the suite, the group, the name, the protocol, the chain |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [alert](alert.md) | `tls/error.h` | the alerts of TLS 1.3, by their numbers on the wire |
| [cipher](cipher.md) | `tls.h` | the cipher suites of TLS 1.3 |
| [group](group.md) | `tls.h` | the key exchange groups: X25519MLKEM768, X25519, P-256, P-384 |

## See also

- [net](../README.md): the [connection](../connection.md) a handshake gives and the [listener](../listener.md)
  `listen` makes; [tcp::connect](../tcp/connect.md) and [tcp::listen](../tcp/listen.md) under them
- [http](../http/README.md): https over this namespace, [client](../http/client.md) and
  [server](../http/server.md)
- [crypto::x509](../../crypto/x509.md): the verification of the chain; [mlkem](../../crypto/mlkem.md),
  [x25519](../../crypto/x25519.md): the key shares
- RFC 8446 (TLS 1.3), RFC 8448 (the traces the handshake is tested against byte for byte), RFC 7301 (ALPN),
  RFC 8449 (record size limit), draft-ietf-tls-ecdhe-mlkem (X25519MLKEM768)
- `tests/net/tls_*.cpp`: the record layer, the key schedule, the messages, both handshake machines against RFC 8448,
  and interoperability with OpenSSL and Go (`tests/net/tls_interop.cpp`); `tests/net/fuzz/tls_*_fuzz.cpp`
