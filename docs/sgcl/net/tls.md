# sgcl::net::tls

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    enum class group : uint16_t;    // x25519_mlkem768, x25519, secp256r1, secp384r1
    enum class cipher : uint16_t;   // aes_128_gcm_sha256, aes_256_gcm_sha384, chacha20_poly1305_sha256
    class identity;                 // a certificate chain with its private key; a handle of one word
    struct config;                  // the settings of a connection: a value
    struct state;                   // what the handshake settled

    expected<net::connection, io::error> connect(const string& address, const config& c = {});
    async::task<expected<net::connection, io::error>> async_connect(string address, config c = {});
    expected<net::connection, io::error> client(const net::connection& transport, const config& c);
    async::task<expected<net::connection, io::error>> async_client(net::connection transport, config c);
    expected<net::listener, io::error> listen(const string& address, const config& c);
    async::task<expected<net::listener, io::error>> async_listen(string address, config c);
    expected<net::connection, io::error> server(const net::connection& transport, const config& c);
    async::task<expected<net::connection, io::error>> async_server(net::connection transport, config c);
    optional<state> state_of(const net::connection& c);
}

#include "sgcl/net/tls/error.h"   // brought in by tls.h

namespace sgcl::net::tls {
    enum class alert : uint8_t;     // the alerts of RFC 8446 §6.2, by their numbers
    const std::error_category& category() noexcept;   // "tls"
    optional<alert> alert_of(const io::error& e) noexcept;
    bool is_remote(const io::error& e) noexcept;
    optional<crypto::x509::reason> certificate_reason(const io::error& e) noexcept;
}
```

TLS 1.3 ([RFC 8446](https://www.rfc-editor.org/rfc/rfc8446)) over the module's [connections](connection.md), as Go's `crypto/tls` gives it, both sides. `net::tls::connect("example.com:443")` dials TCP, completes the handshake and only then gives the connection out: a [`net::connection`](connection.md) like any other, with `read`, `read_line`, `write`, deadlines, `close` and the `async_` forms, so [`net::http`](http/README.md) and every stream of [io](../io/README.md) take it as they take a TCP one. The server's chain is verified by [`crypto::x509`](../crypto/x509.md) against the system's roots or a pool of your own, for the name dialed. `net::tls::listen(":8443", cfg)` is a [`net::listener`](connection.md) whose `accept` gives connections whose handshake is done, with the certificate and key of an `identity`; `net::http::server` serves it as it serves a TCP one (https).

Only TLS 1.3: the groups X25519MLKEM768 (the post-quantum hybrid, first by default), X25519, P-256 and P-384; the cipher suites AES-128-GCM, ChaCha20-Poly1305 and AES-256-GCM; certificates of Ed25519, ECDSA P-256 and P-384, and RSA (PSS). Interoperability was tested against OpenSSL (`s_server`, `s_client`) and Go's `crypto/tls`, each as client and as server: every cipher suite × every group × three kinds of certificate on each side, HelloRetryRequest, ALPN, KeyUpdate and `close_notify`. **The implementation has not been through an independent cryptographic audit.**

## Rules

- **The handshake comes before the connection.** `connect` and `client` return once the handshake is complete and the server's chain verified; `server` once the client's `Finished` is verified; a listener's `accept` gives only connections whose handshake is done. A failure is an `io::error` and no connection. A connection the caller made (`client`, `server`) is closed when its handshake fails.
- **A listener handshakes in tasks of its own.** `listen` binds the address as [`tcp::listen`](socket.md) does and starts an accept loop: each connection's handshake runs in a task of its own, bounded by `config::handshake_timeout`, so a client that is slow, or never speaks, holds no other up. A handshake that fails or times out is dropped: its connection is closed, the alert sent, and nothing reaches `accept` (no log in v1). The ready connections wait for `accept` in a queue of 16. `close()` stops the accept loop, ends the accepts in progress with `io::errc::closed` and closes the connections that were ready and not taken. The listener lives until it is closed.
- **What the server chooses**, from the client's first ClientHello: the first cipher suite of its list the client offers; the first group of its list the client supports and sent a share of, else the first of its list the client supports, asked for with a HelloRetryRequest (Go's rule); the first identity whose certificate is for the name the client sent (SNI), else the first, and of it the first signature scheme of its key the client offers; the first ALPN protocol of its list the client offers (a client that offers some and none of them is refused with `no_application_protocol`; a server with no list answers none). No common suite, group or scheme is `handshake_failure`.
- **A timeout bounds the whole of it.** `config::handshake_timeout` (10 s) covers the TCP connect and the handshake together, the lookup of the name included, and ends them with `ETIMEDOUT` (`is_timeout()`). While the handshake runs it sets the transport's deadlines, and it removes them afterwards: deadlines set on the TLS connection later are the connection's own, as for TCP.
- **Full duplex, as every connection.** One read and one write may run at once, from two tasks or two threads. They share only the keys of the writing direction, taken for one record at a time, so the two interleave by whole records. A read that must answer something (a `KeyUpdate` that asks for one, a fatal alert) takes the writing keys for that one record. A write is cut into records of at most 16 KB, or of the server's `record_size_limit` (RFC 8449). The keys of a direction are updated (§5.5) before they reach their limit.
- **`close()` does not wait; `async_close()` waits at most 100 ms.** Both send `close_notify` and then close the transport. `close()` sends it only when no write holds the record and the socket takes it at once; otherwise it closes without it, as a close from another task has to, since it is how a stuck read or write is cancelled. `async_close()` waits up to 100 ms for the record, then gives the write another 100 ms. `close_write()` sends `close_notify` after the write in progress, then ends the transport's writing half: the peer reads the end, and this side still reads. A write after `close_notify` is `io::errc::closed`.
- **The end of the stream** is the peer's `close_notify`: `read` returns 0 and `read_line` returns `nullopt`. The transport's end at a record's boundary without `close_notify` is also taken as the end, as the TCP connection's end is: a protocol whose messages carry their own lengths (HTTP) sees a cut message as such. The transport's end in the middle of a record is `io::errc::unexpected_eof`.
- **An identity keeps its key out of managed memory.** `net::tls::identity` is a handle of one word; the private key sits in an unmanaged block of its own, is never copied (copies of the handle and of a `config` share it), and is zeroed by the keys' own destructors when the identity is collected. `from_pem` takes the chain (the leaf first) and one key in PEM: PKCS #8 of each kind, SEC 1 (`EC PRIVATE KEY`) or PKCS #1 (`RSA PRIVATE KEY`). It checks the key against the leaf by signing with it and verifying under the leaf's public key. It is the server's; the client of v1 sends no certificate.
- **The defaults are Go's and the browsers'.** Groups in the order X25519MLKEM768, X25519, P-256, P-384. The first ClientHello carries a key share of the hybrid and one of X25519, so a server of either answers in one round trip, and a server of P-256 or P-384 alone costs a HelloRetryRequest. Cipher suites in the order AES-128-GCM, ChaCha20-Poly1305, AES-256-GCM. A `config` narrows both lists and reorders them; the first group given is the one shared first.
- **The middlebox compatibility mode is on** (§D.4): a session id of 32 random bytes and a `change_cipher_spec` record after the first flight, as Go, OpenSSL and the browsers send them. It is not a setting.
- **The server's name** is `config::server_name`, or the host of the address given to `connect`. It is sent as SNI (never for an IP address) and checked against the leaf's names, or its IP addresses for an address. `insecure_skip_verify` takes the chain unchecked. It is for tests: with it, any machine in the path can read and change the traffic.
- **Not in v1:** client certificates (mTLS: a server's `CertificateRequest` is answered with an empty `Certificate`, and the server decides; the server of v1 asks for none), session resumption and PSK (a client reads a `NewSessionTicket` and passes it over; the server sends none), 0-RTT early data (the server refuses it and skips the client's early records, RFC 8446 §4.2.10), and TLS 1.2 or older (a peer without TLS 1.3 fails with `protocol_version`).
- **Errors** are `io::error`s in the category `"tls"`, worded as Go words them. An alert this side sent reads `tls: bad certificate`; the code is the alert's number, so `e.code() == net::tls::alert::decode_error` asks for it. An alert the peer sent reads `remote error: tls: handshake failure`. A chain that did not verify reads `tls: certificate signed by unknown authority`, and `certificate_reason` gives the [`x509::reason`](../crypto/x509.md). The transport's own failures (`ETIMEDOUT`, `ECONNRESET`) stay what they are.

## Members

### connect, client

```cpp
expected<net::connection, io::error> connect(const string& address, const config& c = {});       // TCP, then the handshake
async::task<expected<net::connection, io::error>> async_connect(string address, config c = {});
expected<net::connection, io::error> client(const net::connection& transport, const config& c);   // the handshake over a connection there is
async::task<expected<net::connection, io::error>> async_client(net::connection transport, config c);
```

`connect` takes an address as [`tcp::connect`](socket.md) does, `"host:port"`, and dials it with happy eyeballs. Its blocking form runs on the scheduler and waits for it, so it is for a thread; a task awaits `async_connect`. `client` makes TLS of a connection there is, for a protocol that starts in the clear (STARTTLS) or a transport other than TCP; its `config` must name the server, since there is no address to take the name from.

```cpp
async::task<> fetch() {
    auto c = co_await net::tls::async_connect("example.com:443");
    if (!c) {
        eprintln("{}", c.error().message());   // "tls: certificate signed by unknown authority", "dial tcp …: Operation timed out"
        co_return;
    }
    co_await c->async_write("HEAD / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    auto line = co_await c->async_read_line();
    println("{}", **line);                       // HTTP/1.1 200 OK
    co_await c->async_close();
}
```

### listen, server

```cpp
expected<net::listener, io::error> listen(const string& address, const config& c);                 // TCP, accept after the handshake
async::task<expected<net::listener, io::error>> async_listen(string address, config c);
expected<net::connection, io::error> server(const net::connection& transport, const config& c);   // the server's handshake over a connection there is
async::task<expected<net::connection, io::error>> async_server(net::connection transport, config c);
```

`listen` takes an address as [`tcp::listen`](socket.md) does (`":8443"`, `"127.0.0.1:0"`) and gives a plain `net::listener`: `accept`, `async_accept`, `close`, `local_endpoint`, so `net::http::server::serve` takes it unchanged. `server` makes TLS of a connection there is, one accepted from a listener of TCP or another transport, and returns after the handshake. A server's `config` needs at least one identity; `groups`, `ciphers` and `alpn` are its preferences, in order; `roots`, `server_name` and `insecure_skip_verify` are the client's and are not read.

```cpp
net::tls::config cfg;
cfg.identities = {net::tls::identity(io::read_text("server.pem"), io::read_text("server.key"))};
net::listener l = net::tls::listen(":8443", cfg);
for (;;) {
    auto c = l.accept();                       // its handshake done
    if (!c) {
        break;                                  // io::errc::closed after close()
    }
    async::go(serve_one(*c));
}
```

### config

```cpp
struct config {
    string server_name;                                  // SNI and the name checked; empty: the address's host
    optional<crypto::x509::certificate_pool> roots;      // nullopt: the system's
    vector<tls::identity> identities;                    // a server's
    vector<tls::group> groups = {group::x25519_mlkem768, group::x25519, group::secp256r1, group::secp384r1};
    vector<tls::cipher> ciphers = {cipher::aes_128_gcm_sha256, cipher::chacha20_poly1305_sha256, cipher::aes_256_gcm_sha384};
    vector<string> alpn;                                 // offered, in order of preference (RFC 7301)
    bool insecure_skip_verify = false;                   // tests only
    duration handshake_timeout = 10 * second;            // the connect and the handshake together
};
```

A plain value, copied into the connection. A `config` without groups or cipher suites, a client's without a name to verify (none given and the address a bare port), or a server's without an identity, is refused before anything is sent, with `EINVAL`. So is an ALPN protocol of 0 or more than 255 bytes.

### identity

```cpp
static expected<identity, io::error> from_pem(const string& certificate_chain_pem, const string& key_pem);   // crypto::errc::malformed, op "identity"
explicit identity(const string& certificate_chain_pem, const string& key_pem);                               // the same, std::invalid_argument
const crypto::x509::chain& certificates() const noexcept;                                                    // the leaf first
```

The chain is every `CERTIFICATE` block of the first text, the leaf first. The key is the first block of the second text that reads as a key of a kind TLS 1.3 signs with: `PRIVATE KEY` (PKCS #8: Ed25519, ECDSA P-256 or P-384, RSA), `EC PRIVATE KEY` (SEC 1: P-256 or P-384) or `RSA PRIVATE KEY` (PKCS #1). A key that is not the leaf's (checked by a signature verified under the leaf's public key) is refused. The schemes a key signs CertificateVerify with: Ed25519; ECDSA P-256 with SHA-256; ECDSA P-384 with SHA-384; RSA with RSASSA-PSS over SHA-256, SHA-384 or SHA-512 (`rsa_pss_rsae_*`).

```cpp
net::tls::identity id(io::read_text("server.pem"), io::read_text("server.key"));
```

### state, state_of

```cpp
struct state {
    tls::cipher cipher;
    tls::group group;
    string server_name;                             // a client's: the name checked; a server's: the client's SNI (empty: none sent)
    string alpn;                                    // empty: none
    crypto::x509::chain peer_certificates;          // a client's: the server's chain, the leaf first; a server's: empty (no client certificates in v1)
};

optional<state> state_of(const net::connection& c);   // nullopt for a connection without TLS; a connection from connect, client, server or a TLS listener
```

### alert_of, is_remote, certificate_reason

```cpp
optional<alert> alert_of(const io::error& e) noexcept;                               // the alert, this side's or the peer's
bool is_remote(const io::error& e) noexcept;                                         // an alert the peer sent
optional<crypto::x509::reason> certificate_reason(const io::error& e) noexcept;      // why the chain did not verify
```

```cpp
auto c = net::tls::connect("self-signed.example:443");
if (!c) {
    if (net::tls::certificate_reason(c.error())) {
        eprintln("not trusted: {}", c.error().message());
    } else if (net::tls::is_remote(c.error())) {
        eprintln("the server refused: {}", c.error().message());
    }
}
```

## Example

A server and its client in one program: the server listens with the test certificate of the tree (`tests/net/tls_testdata`: a leaf for localhost, signed by a CA of its own) and answers each line reversed; the client trusts that CA, not the system's roots, and offers the protocol `echo/1`. Run from the root of the tree.

```cpp
#include "sgcl/net/tls.h"
#include "sgcl/io/print.h"

#include <algorithm>

using namespace sgcl;

// The server's side: one connection, each line answered reversed
async::task<> serve(net::listener l) {
    auto c = co_await l.async_accept();                  // its handshake done
    if (!c) {
        co_return;
    }
    for (;;) {
        auto line = co_await c->async_read_line();
        if (!line || !*line) {
            break;                                       // the client's close_notify
        }
        std::string text(line->value().view());
        std::reverse(text.begin(), text.end());
        co_await c->async_write(string(text + "\n"));
    }
    co_await c->async_close();
}

int main() {
    net::tls::config server_cfg;
    server_cfg.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"), io::read_text("tests/net/tls_testdata/ecdsa.key"))};
    server_cfg.alpn = {"echo/1"};
    net::listener listener = net::tls::listen("127.0.0.1:0", server_cfg);
    auto serving = async::spawn(serve(listener));

    net::tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(io::read_text("tests/net/tls_testdata/ca.pem"));
    cfg.alpn = {"echo/1"};
    auto c = net::tls::connect("localhost:" + to_string(listener.local_endpoint().port()), cfg);
    if (!c) {
        eprintln("{}", c.error().message());
        return 1;
    }
    auto s = net::tls::state_of(c);
    println("{} over {}", s->alpn, s->group == net::tls::group::x25519_mlkem768 ? "X25519MLKEM768" : "a classical group");
    println("certificate of {}", s->peer_certificates[0].dns_names()[0]);

    c->write("hello, tls\n");
    auto line = c->read_line();
    println("{}", **line);
    c->close();
    serving.wait();
    listener.close();
}
```

Output:

```text
echo/1 over X25519MLKEM768
certificate of localhost
slt ,olleh
```

## SGCL and Go

| Go | sgcl | note |
|---|---|---|
| `tls.Dial("tcp", a, cfg)`, `tls.DialWithDialer` | `net::tls::connect(a, cfg)`, `async_connect` | the handshake done before it returns; Go's `Dial` does it too, its `Client` waits for the first read or write |
| `tls.Client(conn, cfg)` and `Handshake` | `net::tls::client(transport, cfg)` | one call; the transport closed when the handshake fails |
| `tls.Listen("tcp", a, cfg)`, `tls.NewListener` | `net::tls::listen(a, cfg)`, `async_listen` | `accept` gives connections after their handshake (Go's `Accept` gives them before, the handshake at the first read) |
| `tls.Server(conn, cfg)` and `Handshake` | `net::tls::server(transport, cfg)` | one call |
| `Config.Certificates` | `config::identities` | chosen by SNI, else the first |
| `tls.Config{ServerName, RootCAs, NextProtos, CurvePreferences, InsecureSkipVerify}` | `config{server_name, roots, alpn, groups, insecure_skip_verify}` | `ciphers` too: Go's TLS 1.3 suites are not configurable |
| `tls.LoadX509KeyPair`, `tls.X509KeyPair` | `net::tls::identity(chain_pem, key_pem)`, `identity::from_pem` | the key in unmanaged memory, checked against the leaf |
| `Conn.ConnectionState()` | `net::tls::state_of(c)` | `CipherSuite`, `CurveID`, `NegotiatedProtocol`, `PeerCertificates`, `ServerName` |
| `tls.AlertError`, `"remote error: tls: …"` | `alert_of(e)` and `is_remote(e)`, the same words | one `io::error` for all |
| `tls.CertificateVerificationError` | `certificate_reason(e)` | the reasons of `crypto::x509` |
| `Conn.Close` (sends `close_notify`) | `close()`, `async_close()` | `close()` never waits for a write in progress |
| `Conn.CloseWrite` | `close_write()` | |
| `GetClientCertificate`, `ClientAuth`, `ClientSessionCache`, session tickets, 0-RTT | not in v1 | |

## See also

- [connection](connection.md): what `connect` returns and `listen`'s listener; [socket](socket.md): the TCP connect and listen under them; [error](error.md): the module's other codes
- [http client](http/client.md), [http server](http/server.md): https over this module
- [crypto::x509](../crypto/x509.md): the verification of the chain; [mlkem](../crypto/mlkem.md), [x25519](../crypto/x25519.md): the key shares
- RFC 8446 (TLS 1.3), RFC 8448 (the traces the handshake is tested against byte for byte), RFC 7301 (ALPN), RFC 8449 (record size limit), draft-ietf-tls-ecdhe-mlkem (X25519MLKEM768)
- `tests/net/tls_*.cpp`: the record layer, the key schedule, the messages, both handshake machines against RFC 8448, and interoperability with OpenSSL and Go (`tests/net/tls_interop.cpp`); `tests/net/fuzz/tls_*_fuzz.cpp`
