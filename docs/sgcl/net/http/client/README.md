[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::client

```cpp
#include "sgcl/net/http/client.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class client;
    using dial_function = function<async::task<expected<net::connection, io::error>>(
                                       const net::url&, async::stop_token)>;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::client` is Go's `http.Client` and `http.Transport` in one: HTTP/1.1 and HTTP/2 with a pool of
connections. A request is sent, redirects are followed, and the [response](../response/README.md) comes back with its head read
and its body still on the connection; reading the body to its end gives the connection back to the pool by itself,
where Go asks for both the end and `Close`. A 4xx or a 5xx is a response, not an error, as in Go. The settings are
public fields, as in [io::command](../../../io/command/README.md), read by each request when it starts: `timeout`,
`response_header_timeout`, `idle_timeout` and `max_idle_per_host` are the `Client.Timeout` and the `Transport` fields
of Go of the same meaning, `dial` its `DialContext`, `max_redirects` and `follow_redirects` what Go's `CheckRedirect`
decides.

A copy of a client shares its pool and carries its own settings, copied with it. Over TLS it speaks HTTP/2 when the
server chooses it (RFC 9113, as Go's `Transport` does by default): one connection per origin shared by the requests, a
stream each, with nothing of the program's code changed but what [proto](../response/proto.md) of the response says.
[download](download.md) is curl's `-fo path url`; the free [download](../download.md) does the same through a
client of the process's, with the default settings.

A request goes through the proxy the environment names (`http_proxy`, `HTTPS_PROXY`, `ALL_PROXY`, `NO_PROXY`, read when
the client is made), or the one in its member `proxy`: an HTTP proxy, one over TLS, or SOCKS5
([proxy](../proxy/README.md)). Cookies are kept when the member `jar` holds a [cookie_jar](../cookie_jar/README.md),
none by default as in Go's `Client.Jar`. There is no gzip in this version: the client sends no `Accept-Encoding`, so servers do
not compress.

## Rules

- **Two forms.** [get](get.md), [head](head.md), [post](post.md), [send](send.md) and
  [download](download.md) block the calling thread: the exchange runs on the scheduler and the thread waits
  for it, so they are for a thread of the program, never a worker. A task writes `co_await web.async_get(url)`, and
  reads the body with `co_await res.async_text()`. The forms for a task throw nothing: a URL that does not parse, or
  one past 512 MiB ([the limit](../../url/README.md#rules)), is the error `net::errc::invalid_url`.
- **The pool** is keyed by the origin (scheme, host, port). Idle connections are taken last in first out, at most
  `max_idle_per_host` of them are kept (16; Go keeps 2), and one idle for `idle_timeout` (90 s) is closed by a timer
  of the module's clock. A connection goes back when the body of its response was read to its end and the server did
  not say `Connection: close`; it is closed when anything went wrong. One whose body was not read stays out:
  [close](../response/close.md) of the response gives it back when the rest is already in the buffer, and closes it
  otherwise.
- **Retries.** A request on a pooled HTTP/1.1 connection that fails before a byte of the response (the server closed
  the connection while it was idle) is sent once more on a new one, for an idempotent method or a body held in memory
  (RFC 9110 §9.2.2), as Go does. Over HTTP/2 a stream the server never processed goes again by itself, up to two more
  times: one it refused (`REFUSED_STREAM`, RFC 9113 §8.7) for any method, one above the last stream of its GOAWAY
  (§6.8) or lost with its connection before the head only for an idempotent method (Go sends any method again after a
  GOAWAY; this version does not). A body given as a stream is never sent twice.
- **Redirects**, up to `max_redirects` (10; `net::errc::too_many_redirects` past it): 301, 302 and 303 go on as GET
  without a body (HEAD stays HEAD, and 301 and 302 keep a GET), 307 and 308 keep the method and the body, unless the
  body is a stream (the 307 is then the response). `Authorization`, `Cookie`, `Proxy-Authorization` and
  `WWW-Authenticate` do not follow to a host that is neither the same nor under it. A redirect may go from `http` to
  `https` and back, as in Go. The response's [url](../response/url.md) is the last URL. With `follow_redirects` off
  none is followed: the 3xx is the response, its `Location` and its body as they came (Go's `CheckRedirect` returning
  `ErrUseLastResponse`).
- **Cookies**: with a `jar`, the `Set-Cookie` of every response, each redirect's among them, goes into the jar
  ([set_cookies](../cookie_jar/set_cookies.md)), and every request, each redirect's among them, carries the jar's
  cookies for its URL ([header](../cookie_jar/header.md)) in one `Cookie` field after the request's own pairs, as
  Go's client merges them; a pair of the request's own whose name a response of the same exchange set again is left
  out after it, the jar's being the one sent. The same over HTTP/2, and for the handshake of
  [websocket](websocket.md). Without a jar (the default) nothing is kept, and a request's own `Cookie` goes as it is.
  A copy of a client carries the same jar; one jar may serve many clients at once.
- **Timeouts**: `timeout` bounds the whole exchange, the reading of the body included; `connect_timeout` the dial;
  `response_header_timeout` the wait for the head after the request went out. Each is `ETIMEDOUT`
  ([is_timeout](../../../io/error/is_timeout.md)). Over HTTP/2 they hold for the request's stream alone: past one the
  stream is reset (`CANCEL`) and the connection goes on carrying the other requests.
- **What is sent**: `Host` from the URL (unless the request sets one), the program's fields, a `Content-Length` for a
  body in memory, a [form](../form/README.md) (its files' sizes taken when the send begins) or a stream of a known length, chunked for a stream without one, `Content-Length: 0` for a POST, PUT
  or PATCH without a body. No `Accept-Encoding` and no `User-Agent` unless the program sets one.
- **What cannot be sent** is refused before a connection is dialed: a method that is not a token, a field name that is
  not one, a value with CR, LF, NUL or another control (obs-fold among them), a target or a `Host` with a space or a
  control make the send `std::errc::invalid_argument`, the part named (`invalid header value: X-Foo`): not an
  exception and not a byte on the wire.
- **What is read**: the response is held to RFC 9112 as a request is on the server (a `Transfer-Encoding` with a
  `Content-Length`, two lengths, a coding other than chunked, a broken chunk: `net::errc::malformed_response`); 1xx
  responses before the final one are skipped (100 Continue, 103 Early Hints); a response to HEAD, a 204 and a 304 have
  no body; one with neither a length nor chunked lasts to the close, and its connection is not kept. A head past
  `max_response_header_bytes` (1 MB) is `net::errc::header_too_large`.
- **https://** is over [TLS 1.3, or TLS 1.2 to a server without 1.3](../../tls/README.md) (`tls.min_version` set to
  `tls13` requires 1.3; HTTP/2 goes over either, every suite being ECDHE and an AEAD, RFC 7540 §9.2): the connection is
  made by [net::tls::connect](../../tls/connect.md)
  with the `tls` settings (the system's roots unless `tls.roots` names others), port 443 unless the URL gives one.
  ALPN offers `h2` first and then what `tls.alpn` holds (`http/1.1`); `http2 = false` leaves `h2` out. The server's
  name is the URL's host, an IP address checked against the certificate's addresses; `tls.server_name` overrides it.
  A certificate that does not verify fails the request with the TLS error (`tls: certificate signed by unknown
  authority`, [certificate_reason](../../tls/certificate_reason.md)). A `dial` function given makes the transport and
  TLS goes over it, as over Go's `DialContext`. The pool keeps https and http connections apart (the origin includes
  the scheme).
- **Authentication**: with `credentials` (or a request's own, [set_credentials](../request/set_credentials.md)), a 401
  of the origin of the request's own URL is answered once: Digest (RFC 7616) when a challenge offers it, the strongest
  algorithm of SHA-512/256, SHA-256 and MD5 with qop `auth` (`auth-int` when only that is offered and the body is in
  memory; `username*` for a name past ASCII, `userhash` when asked), else Basic (RFC 7617). The protection space — the
  origin, the realm and the paths under the directory of the URL that was asked — is kept in the client's pool, so the
  next requests under it send `Authorization` at once (Digest: the same nonce, its count grown, a new client nonce); a
  401 with `stale=true` is answered with the new nonce, without the password failing. Credentials refused give the 401,
  after the one retry. They never go to another origin: a redirect to one carries none, and a URL's
  `user:password@` is sent at once as Basic, as Go sends it, to its own origin alone.
- **Proxies**: `proxy` is the environment's when the client is made ([from_environment](../proxy/from_environment.md));
  `web.proxy = net::http::proxy(url)` sends every request through one, `net::http::proxy()` none. An `http://` request
  goes to an HTTP proxy in absolute-form, an `https://` one through a `CONNECT` tunnel with TLS and HTTP/2 to the origin
  over it, every request through a SOCKS5 proxy's tunnel; the pool keeps each route's connections apart, and the
  errors of the way through name the proxy ([the rules](../proxy/README.md#rules)). The `dial` function, when there is
  one, dials the proxy, given its URL.
- **HTTP/2** comes when the server chooses `h2` by ALPN, or on `http://` by prior knowledge when `h2c` is on (the
  preface sent at once, no Upgrade: for a server known to speak it, such as a service behind the program's own
  proxy). The origin gets one connection, shared by every request to it, each on a stream of its own; a second one is
  dialed only when the first carries as many streams as the server's `SETTINGS_MAX_CONCURRENT_STREAMS` allows.
  Requests that find no connection wait for the one being dialed rather than each dialing its own. A new connection
  to an origin starts from the limit its server already told another connection, and a first one from 100 until the
  server's SETTINGS come, as Go's does. A request's body goes as DATA within the server's windows; a response's body
  is read within ours: 1 MB a stream and 16 MB the connection. A body nobody reads stops its stream at 1 MB, and the
  other streams go on (Go's 4 MB and 1 GB were not taken, since memory comes first; 1 MB at 100 ms of RTT is some
  80 Mb/s a stream). The response's fields are held to RFC 9113 §8.3 (`:status` alone and first, names in lower case,
  no fields of HTTP/1.1's connection; else `net::errc::malformed_response`), a list past
  `max_response_header_bytes` is `net::errc::header_too_large`, and 1xx responses are skipped as over HTTP/1.1. A
  connection without streams for `idle_timeout` is closed with GOAWAY, and so is one that is idle when
  [close_idle_connections](close_idle_connections.md) is called. A server that chooses `http/1.1` gets
  HTTP/1.1 and its pool.
- **Not in this version** (HTTP/2): server push (the client sends `ENABLE_PUSH = 0`), priorities (RFC 9218),
  `CONNECT` and Extended CONNECT (WebSocket over HTTP/2), trailers on a request, a PING to test an idle connection and
  a write deadline of the connection's own (Go's `ReadIdleTimeout` and `WriteByteTimeout`, off by default there too):
  a connection whose server has gone silent is found only by the timeouts of the requests on it.
- A scheme other than `http` and `https` is `net::errc::unsupported_scheme`; a URL that does not parse is
  `net::errc::invalid_url` ([errc](../../errc.md)). Every error is an [io::error](../../../io/error/README.md) that names the
  method and the URL (`GET http://127.0.0.1:1/: Connection refused`).

## Member objects

| Member | Description |
|---|---|
| `duration timeout` | bounds the whole exchange, the redirects and the reading of the body included (Go's `Client.Timeout`); zero, the default, is none |
| `duration connect_timeout` | bounds the dial of a connection, the TLS handshake included when it is shorter than `tls.handshake_timeout`; 30 s by default |
| `duration response_header_timeout` | bounds the wait for the head of the response, from the request sent (Go's `ResponseHeaderTimeout`); zero, the default, is none |
| `duration idle_timeout` | how long a connection stays idle in the pool before it is closed (Go's `IdleConnTimeout`); 90 s by default |
| `size_t max_idle_per_host` | the most idle connections kept for one origin (Go's `MaxIdleConnsPerHost`); 16 by default, where Go keeps 2 |
| `int max_redirects` | the most redirects followed before `net::errc::too_many_redirects`; 10 by default |
| `bool follow_redirects` | whether a redirect is followed; `true` by default. `false` hands back the 3xx itself, its `Location` for the program to read (Go's `CheckRedirect` returning `ErrUseLastResponse`), its `Set-Cookie` still kept in the `jar` |
| `size_t max_response_header_bytes` | the most bytes of a response's head, past which it is `net::errc::header_too_large`; 1 MB by default, zero taken as 1 |
| `dial_function dial` | how a connection is made, given the URL and a stop token stopped at the connect's timeout (Go's `DialContext`): a unix socket, a connection in memory; for https the transport under TLS. Empty, the default, is [tcp::connect](../../tcp/connect.md) |
| `net::tls::config tls` | the [TLS settings](../../tls/config.md) of https: roots, groups, cipher suites, `insecure_skip_verify`, `handshake_timeout`, a client certificate in `identities`; the server name is the URL's host when none is set. By default the default config with ALPN `http/1.1` and a [session_cache](../../tls/session_cache/README.md) of the client's own, so that its connections to a server resume each other's sessions, over 1.3 or 1.2; `tls.session_cache = nullopt` turns resumption off, and one cache given to several clients shares their sessions |
| `bool http2` | for https, `h2` offered first by ALPN; `true` by default |
| `bool h2c` | `http://` sent as HTTP/2 by prior knowledge; `false` by default. Read when a connection is dialed: an idle HTTP/1.1 connection of the pool is still taken as it is |
| `http::proxy proxy` | the [proxy](../proxy/README.md) of each request: `http`, `https` and `no_proxy`; by default the environment's as the client is made ([from_environment](../proxy/from_environment.md)) |
| `optional<cookie_jar> jar` | the [cookie_jar](../cookie_jar/README.md) that keeps the responses' cookies and gives the requests theirs (Go's `Client.Jar`); `nullopt`, the default, keeps none |
| `bool decompress` | `Accept-Encoding: gzip, deflate, br, zstd` sent on a request without one of its own and without `Range`, and the body decoded as it is read: the response's `Content-Encoding` and `Content-Length` removed, [uncompressed](../response/uncompressed.md) `true` (Go's transparent gzip). `true` by default; `false` sends and decodes nothing (Go's `DisableCompression`) |
| `optional<http::cache> cache` | the private [cache](../cache/README.md) of the client's GET and HEAD (RFC 9111): a fresh stored response served with no request sent, a stale one asked again with its validators, `stale-while-revalidate` and `stale-if-error`; how each response came is its [from_cache](../response/from_cache.md). `nullopt` by default: no cache |
| `optional<http::credentials> credentials` | the [credentials](../credentials.md) a 401 of the origin of a request's own URL is answered with, once: Digest when offered, else Basic; the protection space then remembered, its next requests sent with `Authorization` at once. `nullopt`, the default: a 401 is the response |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | constructs a client with an empty pool and the default settings |
| `(destructor)` | drops the client; the pool stays while a copy holds it |
| `operator=` | copies another client: the pool shared, the settings copied; a move is the copy, so a moved-from client is the same client |

#### Requests

| Function | Description |
|---|---|
| [send, async_send](send.md) | sends a request and returns the response |
| [get, async_get](get.md) | sends a GET |
| [head, async_head](head.md) | sends a HEAD |
| [post, async_post](post.md) | sends a POST with a body, or a form with its files |
| [download, async_download](download.md) | sends a GET and saves a 2xx body to a file |
| [websocket, async_websocket](websocket.md) | a WebSocket through this client's proxy, TLS settings and dial |

#### Pool

| Function | Description |
|---|---|
| [close_idle_connections](close_idle_connections.md) | closes the idle connections of the pool now |

## Example

A server of the program answers on a port of its own, and the client asks it:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.write("hello\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::response res = web.get(base + "/hello");
    print("{} {}", res.status(), res.text().value());
    net::http::response missing = web.get(base + "/nothing");
    println("{} {}", missing.status(), missing.ok());
    missing.close();
    srv.close();
}
```

Output:

```text
200 hello
404 false
```

A client that trusts a CA of its own, against a server of the same program with the test certificate of the tree
(`tests/net/tls_testdata`: a CA and a leaf for localhost); with the system's roots it is refused. The server offers
`h2` by ALPN, and the client speaks HTTP/2 to it.

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::config tls;
    tls.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                                         crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    tls.alpn = {"h2", "http/1.1"};
    net::http::server srv;
    srv.route("GET /hello", [](net::http::request, net::http::response_writer w) {
        w.write("hello over TLS\n");
    });
    net::listener listener = net::tls::listen("127.0.0.1:0", tls);
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "https://localhost:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    auto refused = web.get(base + "/hello");
    println("{}", refused.error().code().message());

    web.tls.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    net::http::response res = web.get(base + "/hello");
    print("{} {}", res.proto(), res.text().value());
    srv.close();
}
```

Output:

```text
tls: certificate signed by unknown authority
HTTP/2.0 hello over TLS
```

Over plain TCP a server with `h2c` on answers HTTP/2 by prior knowledge and HTTP/1.1 on the same port, and a client
with `h2c` on sends `http://` as HTTP/2:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.h2c = true;
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        w.write("asked over " + req.proto() + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client plain;
    print("{}", plain.get(base + "/")->text().value());
    net::http::client multiplexed;
    multiplexed.h2c = true;
    print("{}", multiplexed.get(base + "/")->text().value());
    srv.close();
}
```

Output:

```text
asked over HTTP/1.1
asked over HTTP/2.0
```

## See also

- [request](../request/README.md), [response](../response/README.md): what is sent and what comes back; [server](../server/README.md): the other
  side
- [download](../download.md): a file in one line, through a client of the process's
- [url](../../url/README.md): how the URL is read; [connection](../../connection/README.md): what `dial` returns
- [tls](../../tls/README.md): the connection under https and its errors
- [proxy](../proxy/README.md): the proxies, from the environment or given
- [cookie_jar](../cookie_jar/README.md): the cookies, kept when `jar` holds one
- [test_server](../test_server/README.md): a server for a test, and a client made for it
- `tests/net/http/client.cpp` (a scripted server in memory: the pool, the retry, every framing, redirects, errors),
  `tests/net/http/download.cpp` (`download` through the part file, a cut body, a status but 2xx, `save`,
  `json<T>`), `tests/net/http/go.cpp` (against Go's server), `tests/net/http/https.cpp` (https: against this
  module's server, Go's net/http over TLS and curl), `tests/net/http/h2_client.cpp` (HTTP/2 against Go's server by
  `tools/h2_oracle.go`, and a scripted server for REFUSED_STREAM, GOAWAY and a connection cut),
  `tests/net/http/h2_client_connection.cpp` (the client's side of the HTTP/2 machine, frame by frame, by the sections
  of RFC 9113), `tests/net/http/proxy.cpp` (the proxies)
