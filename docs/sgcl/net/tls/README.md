[sgcl](../../README.md) › [net](../README.md) › tls

# sgcl::net::tls

```cpp
#include "sgcl/net/tls.h"   // namespace sgcl::net::tls
```

TLS 1.3 ([RFC 8446](https://www.rfc-editor.org/rfc/rfc8446)) over the module's [connections](../connection/README.md), both
sides, as Go's `crypto/tls` gives it, and TLS 1.2 ([RFC 5246](https://www.rfc-editor.org/rfc/rfc5246)) for a client
whose server has no 1.3. [connect](connect.md) dials TCP, completes the handshake and only then gives the
connection out: a [net::connection](../connection/README.md) like any other, with `read`, `read_line`, `write`, deadlines,
`close` and the `async_` forms, so that [http](../http/README.md) and every stream of [io](../../io/README.md)
take it as they take a TCP one. The server's chain is verified by [crypto::x509](../../crypto/x509.md) against the
system's roots (on macOS the Keychain's) or a pool of the program's, for the name dialed, and checked for revocation
when the config asks: the OCSP response the server staples, CRLs, OCSP online ([revocation_mode](revocation_mode.md)). [listen](listen.md) is a
[net::listener](../listener/README.md) whose `accept` gives connections whose handshake is done, with the certificate and
the key of an [identity](identity/README.md); [http::server](../http/server/README.md) serves it as it serves a TCP one
(https). [client](client.md) and [server](server.md) are the two handshakes over a connection the program already
has. A server may ask the client for a certificate of its own ([client_auth](client_auth.md), mTLS), and the
client answers with an identity of its config; a server issues session tickets under its
[ticket_keys](ticket_keys/README.md), which a client with a [session_cache](session_cache/README.md) resumes on its next
connection without the certificates and their signatures, as it resumes the tickets and the session ids of a server
of 1.2. A server staples the OCSP response of its leaf to its
chain (RFC 6066, RFC 8446 §4.4.2.1), given by the program or fetched and refreshed by itself.

TLS 1.3 both ways, and the client's TLS 1.2 ([version](version.md)): the groups X25519MLKEM768 (the post-quantum
hybrid, first by default, 1.3's alone), X25519, P-256 and P-384 ([group](group.md)); the cipher suites
AES-128-GCM, ChaCha20-Poly1305 and AES-256-GCM, over 1.2 with ECDHE signed by an ECDSA or an RSA certificate
([cipher](cipher.md));
certificates of Ed25519, ECDSA P-256 and P-384, and RSA (PSS), the server's and the client's; resumption by
session tickets with a fresh key exchange (`psk_dhe_ke`), no 0-RTT, and the client's resumption of 1.2 by tickets
(RFC 5077) and session ids. Interoperability was tested against OpenSSL
(`s_server`, `s_client`) and Go's `crypto/tls`, each as client and as server: every cipher suite × every group ×
three kinds of certificate on each side, HelloRetryRequest, ALPN, KeyUpdate and `close_notify`, client
certificates and resumed sessions both ways; the client's 1.2 against Go's server of 1.2 and `s_server -tls1_2`,
every suite × every curve × every kind of certificate, the signature schemes of 1.2, ALPN `h2`, client
certificates, sessions resumed by ticket and by session id. **The
implementation has not been through an independent cryptographic audit.**

The settings of a connection are one value, a [config](config.md), Go's `tls.Config`; what the handshake settled
is another, a [state](state.md), read back by [state_of](state_of.md). A failure is an
[io::error](../../io/error/README.md) like every other of the module, in a category of its own, `"tls"`
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
   before they reach their limit. A failure belongs to its direction. A write the transport refuses (the peer
   gone, a write deadline, the `close_notify` of `close_write()`) fails, and so does every write after it, while
   the reads go on with what the transport gives: the records already received, then its end or its error. A
   read that fails on what it received fails, and so do every read and every write after it, with the read's
   error: a record that does not open is answered with this side's alert, the peer's fatal alert with nothing.
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
   name the client sent (SNI), else the first — or the one `config::identity_for` gives for the hello, which the
   handshake waits for ([client_hello](client_hello.md)) — and of it the first signature scheme of its key the client offers;
   the first ALPN protocol of its list the client offers (a client that offers some and none of them is refused with
   `no_application_protocol`; a server with no list answers none). No common suite, group or scheme is
   `handshake_failure`.
7. The defaults are Go's and the browsers'. A client offers 1.3 and 1.2 in one ClientHello (`config::min_version`
   and `max_version`, [version](version.md)); a server without 1.3 answers in 1.2, and `min_version = tls13` refuses
   it. Groups in the order X25519MLKEM768, X25519, P-256, P-384. The first
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
10. A private key never lies in managed memory. An [identity](identity/README.md) keeps its key in an unmanaged block of
    its own, never copied, and zeroed when the identity is collected; its PEM is read where it lies, from a
    [secret_bytes](../../crypto/secret_bytes/README.md) of [crypto::read_secret](../../crypto/secret/README.md). The same
    holds for the secrets of resumption: a session's key in a client's cache and the keys a server seals its tickets
    under lie in unmanaged blocks, zeroed when a session is used, dropped or cleared and when a key is replaced.
11. The values of the namespace hold tracked pointers: an [identity](identity/README.md), a [revocation_cache](revocation_cache/README.md), a
    [session_cache](session_cache/README.md) and [ticket_keys](ticket_keys/README.md) are handles of one word, and a
    [config](config.md) and a [state](state.md) hold vectors, strings and certificates. They live where a
    `tracked_ptr` may: on a stack, in a task, in a managed object; in a global, under a
    [rooted](../../core/rooted/README.md).
12. A client certificate is the server's to ask for: `config::client_auth` `request` or `require`
    ([client_auth](client_auth.md)), the chain verified for client authentication against `config::client_roots`
    (the system's by default), whose authorities the server names; a client answers with the first of its
    `identities` whose key signs a scheme the server takes and whose chain one of those authorities issued, or with
    none. In TLS 1.3 the client's handshake is done before the server has read its certificate, so a client the
    server refuses learns it at its first read: `remote error: tls: certificate required` (or `unknown certificate
    authority`, `bad certificate`); the server's `accept` never gives such a connection.
13. A resumed session skips the certificates, never the key exchange. A server with `config::session_tickets` (on
    by default) sends one ticket with its first write after the handshake, sealed under its
    [ticket_keys](ticket_keys/README.md) and good for `config::ticket_lifetime` (a day by default, seven at most); a
    client with a [session_cache](session_cache/README.md) keeps it, by the server's name, port and ALPN protocols,
    and offers it once on its next connection there. The server takes it when its key still opens it, its lifetime
    is not over, its suite is of the hash the server chose and, where a client certificate is required, the session
    has one; anything else is a full handshake, and a ticket whose binder does not verify ends the handshake with
    `decrypt_error`. The [state](state.md) says `resumed` on both sides, and keeps the certificates of the handshake
    that made the session. Not here: 0-RTT early data (the server refuses it and skips the client's early records,
    RFC 8446 §4.2.10) and session ids and stateful caches on the server.
14. The client speaks TLS 1.2 to a server without 1.3; the server speaks 1.3 alone, and a client of 1.2 alone fails
    with `protocol_version`, as does any peer of 1.1 or older. The client's 1.2 is ECDHE (X25519, P-256, P-384) and
    the six AEAD suites of [cipher](cipher.md), no RSA key transport, no DHE, no CBC; the ServerKeyExchange's
    signature (PKCS #1 v1.5, PSS, ECDSA, Ed25519) checked under the verified leaf, whose key must be the suite's
    kind; the extended master secret of RFC 7627 required (a server without it is refused with
    `handshake_failure`, as a modern client may: without it a connection's keys can be made the same as another's);
    RFC 5746's renegotiation_info, empty, and no renegotiation (a server's HelloRequest is passed over); client
    certificates as in 1.3, the CertificateVerify over the handshake's messages; ALPN, `h2` included (RFC 7540
    §9.2 asks for ECDHE and an AEAD, which every suite has). A client that offered 1.3 and reads RFC 8446 §4.1.3's
    downgrade sentinel in a 1.2 ServerHello refuses it with `illegal_parameter`. A client with a
    [session_cache](session_cache/README.md) resumes 1.2 sessions too (RFC 5246 §7.3): the server's ticket
    (RFC 5077, asked for while `config::session_tickets` is on) or, without one, the session id it named, kept
    in the same cache as 1.3's and offered on the next connection, a ticket with a fresh random session id beside
    it. A server that echoes the session id makes the abbreviated handshake: no certificates and no key exchange,
    the keys made of the session's master secret and the new randoms, the server's Finished first; the session's
    suite, else `illegal_parameter`, and the extended master secret, else `handshake_failure` (RFC 7627 §5.3);
    the [state](state.md) says `resumed` and keeps the session's chain and curve. A server that does not echo it
    makes a full handshake, and its session replaces the one offered. A 1.2 session is kept again after each
    resumption, a renewed ticket in place of the old (RFC 5077 §3.3), and lives for the ticket's lifetime hint,
    seven days at most, or a day where the server names none (RFC 5246 §F.1.4), counted from the full handshake
    that made it.
15. Revocation is the config's to ask for, `off` by default as in Go ([revocation_mode](revocation_mode.md)). With
    `config::revocation` set, a client asks for a staple (`status_request`, RFC 6066) in TLS 1.3 and 1.2 (the
    `CertificateStatus` message), and once the chain verified checks it, before the connection is given out: the
    leaf by the staple, verified under its issuer, every certificate but the root by the config's CRLs, and with
    `soft_fail` and `hard_fail` by the OCSP responders and CRL distribution points the certificates name, fetched
    over plain HTTP within `revocation_timeout` and kept in a [revocation_cache](revocation_cache/README.md). A
    revoked certificate ends the connection with `certificate_revoked`, a staple that does not verify and a leaf of
    OCSP Must-Staple (RFC 7633) without one with `bad_certificate_status_response`, a status `hard_fail` cannot have
    with `certificate_unknown`; the error's [certificate_reason](certificate_reason.md) is `revoked` or
    `revocation_unknown`. The [state](state.md) says the leaf's status and its source. A server with
    `client_auth` checks a client's chain the same way, but for the staple; it staples the response of its
    identity's leaf (an [identity](identity/README.md)'s, or fetched with `config::ocsp_stapling`) to a client that
    asks. Not here: the stapling of several responses (RFC 6961), a client's staple of its own certificate, OCSP
    over https and indirect CRLs.
16. The system's roots are, on macOS, the Keychain's trust settings: the system's anchors less those an
    administrator or the user distrusted, with those they trusted for TLS added
    ([certificate_pool::system](../../crypto/x509-certificate_pool/system.md)), read once through Security.framework,
    which the library links there (CMake's target does it); elsewhere, and where `SSL_CERT_FILE` or `SSL_CERT_DIR` is
    set, the bundle files of the system.
17. Errors are `io::error`s in the category `"tls"`, worded as Go words them. An alert this side sent reads
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
| [client_hello](client_hello.md) | `tls.h` | what a client's hello asks a server for: the name and the protocols, given to `config::identity_for` |
| [config](config.md) | `tls.h` | the settings of a connection, a value: Go's `tls.Config` |
| [identity](identity/README.md) | `tls.h` | a certificate chain with its private key, a server's or a client's, a handle of one word; its OCSP staple |
| [revocation_cache](revocation_cache/README.md) | `tls.h` | what the online checks of revocation fetched, OCSP answers and CRLs, until their `nextUpdate` |
| [session_cache](session_cache/README.md) | `tls.h` | the sessions a client resumes, by server: Go's `ClientSessionCache` |
| [state](state.md) | `tls.h` | what the handshake settled: the suite, the group, the name, the protocol, the chain, the resumption |
| [ticket_keys](ticket_keys/README.md) | `tls.h` | the keys a server seals its session tickets under, rotated |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [alert](alert.md) | `tls/error.h` | the alerts of TLS 1.3, by their numbers on the wire |
| [cipher](cipher.md) | `tls.h` | the cipher suites: TLS 1.3's, and the client's TLS 1.2 ones |
| [client_auth](client_auth.md) | `tls.h` | whether a server asks the client for a certificate |
| [group](group.md) | `tls.h` | the key exchange groups: X25519MLKEM768, X25519, P-256, P-384 |
| [revocation_mode](revocation_mode.md) | `tls.h` | whether and how the peer's chain is checked for revocation: off, staple_only, soft_fail, hard_fail |
| [revocation_source](revocation_source.md) | `tls.h` | where a connection's revocation status came from: the staple, OCSP, a CRL |
| [version](version.md) | `tls.h` | the versions: TLS 1.2 (the client's) and 1.3 |

## Objects and types

| Name | Header | Description |
|---|---|---|
| `identity_function` | `tls.h` | `function<async::task<expected<identity, io::error>>(const client_hello&)>`: a server's identity for a hello, the type of `config::identity_for` ([client_hello](client_hello.md)) |

## See also

- [net](../README.md): the [connection](../connection/README.md) a handshake gives and the [listener](../listener/README.md)
  `listen` makes; [tcp::connect](../tcp/connect.md) and [tcp::listen](../tcp/listen.md) under them
- [http](../http/README.md): https over this namespace, [client](../http/client/README.md) and
  [server](../http/server/README.md)
- [crypto::x509](../../crypto/x509.md): the verification of the chain; [mlkem](../../crypto/mlkem.md),
  [x25519](../../crypto/x25519.md): the key shares
- RFC 8446 (TLS 1.3), RFC 5246 (TLS 1.2), RFC 7627 (the extended master secret), RFC 8422 (ECDHE and ECDSA in
  1.2), RFC 5746 (renegotiation_info), RFC 5077 (the session tickets of 1.2), RFC 8448 (the traces the handshake and the PSK binder are tested against byte for byte), RFC 7301 (ALPN),
  RFC 8449 (record size limit), draft-ietf-tls-ecdhe-mlkem (X25519MLKEM768), RFC 6066 (status_request), RFC 6960
  (OCSP), RFC 5280 (CRLs), RFC 7633 (Must-Staple)
- `tests/net/tls_revocation.cpp`: revocation and stapling against OpenSSL's responder and `s_server -status_file`
  and Go's crypto/tls
- `tests/net/tls_*.cpp`: the record layer, the key schedule, the messages, both handshake machines against RFC 8448,
  client certificates and resumption (`tls_auth.cpp`, `tls_resumption.cpp`), and interoperability with OpenSSL and
  Go (`tests/net/tls_interop.cpp`); `tests/net/fuzz/tls_*_fuzz.cpp`
