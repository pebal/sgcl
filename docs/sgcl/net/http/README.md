[sgcl](../../README.md) › [net](../README.md) › http

# sgcl::net::http

```cpp
#include "sgcl/net/http.h"   // namespace sgcl::net::http
```

What Go has in `net/http` for HTTP/1.1 and HTTP/2, and WebSocket ([websocket](websocket/README.md)) and
Server-Sent Events ([event_stream](event_stream/README.md), [event_source](event_source/README.md)), which Go leaves
outside its standard library: a [client](client/README.md) with a pool of connections, a
[server](server/README.md) with the routes of Go 1.22's `ServeMux`, the messages both of them handle ([request](request/README.md),
[response](response/README.md), [response_writer](response_writer/README.md)), [headers](headers/README.md), [cookies](cookie/README.md) with
a client's [cookie_jar](cookie_jar/README.md) and the [Public Suffix List](public_suffix.md) it stands on, and the
codes of [status](status.md); files and content answered with validators, conditional requests and ranges
([serve_file](serve_file.md), [serve_content](serve_content.md), [file_server](file_server/README.md)); the
[middlewares](middleware.md) of a server (CORS, recovery, a log, a body's limit, a rate limit, sessions, CSRF, Basic and Digest
authentication, which the client answers too, response compression, which the client decodes); a [reverse_proxy](reverse_proxy/README.md) with balancing and health checks, and what a
test of HTTP code needs, Go's `httptest`: a [test_server](test_server/README.md) on the loopback, a
[response_recorder](response_recorder/README.md) and a [test_request](test_request.md). It stands on the rest of [net](../README.md) (connections, [URLs](../url/README.md)), on
[async](../../async/README.md) and on [io](../../io/README.md). `https://` goes over [TLS 1.3](../tls/README.md) (or 1.2, to a server
without 1.3), and
HTTP/2 (RFC 9113) comes by ALPN over TLS, or by prior knowledge on a plain port (`h2c`), under the same types and
handlers.

The idea it rests on is that an exchange is a task on the scheduler and the program chooses how to wait for it. A
handler is a plain function of `(request, response_writer)` when it never waits — a write only buffers, and the
server sends the response whole with its Content-Length when the handler returns — and a task when it reads a body
or calls another service; a client's call is the task started and waited for on a thread, or awaited in a task. The
messages are handles, so a request, a response or a writer passed into a task keeps what it needs alive, and the
bytes of the wire go through one buffer per connection, from which a head is copied once into a string whose slices
its fields are.

Errors are values, an `expected<T, io::error>` ([io::error](../../io/error/README.md)). The codes of the module are
[net::errc](../errc.md)'s: `invalid_url`, `unsupported_scheme`, `malformed_response`, `header_too_large`,
`body_too_large`, `too_many_redirects`, `server_closed`, `invalid_cookie`, `http_status`, and a proxy's
(`proxy_refused`, `proxy_auth_required`, `malformed_proxy_response`) and a multipart body's
(`malformed_multipart`, `too_many_parts`, `not_multipart`) and a WebSocket's (`websocket_handshake`,
`websocket_protocol`, `websocket_closed`), each in an `io::error` that
names the method and the URL (`GET http://x/: connection refused`). A 4xx or a 5xx is a response, not an error.

## The rules

1. As everywhere in io and net, an operation without a prefix does its work on the calling thread and returns the
   result, and the form for a task has the prefix `async_` and returns a task: `web.get(url)` and `co_await
   web.async_get(url)`, `res.text()` and `co_await res.async_text()`, `server.serve(":8080")` and `co_await
   server.async_serve(":8080")`. The exchange itself always runs on the scheduler, so a synchronous call is the task
   started and waited for: it is for a thread of the program (`main`), never for a worker. A handler runs on a
   worker, so a handler that reads a body or waits for anything is a task and uses the `async_` forms.
2. A [request](request/README.md), a [response](response/README.md) and a [response_writer](response_writer/README.md) are handles of one
   word, a `tracked_ptr` to the message, as a [connection](../connection/README.md) is: a copy shares the message, and a
   handle passed by value into a task keeps it alive. A [client](client/README.md) and a [server](server/README.md) are a word to
   what the copies share (the client's pool, the server's routes and connections) and their settings, which are each
   copy's own. A [cookie_jar](cookie_jar/README.md) is a word to the cookies its copies share. All of them hold a
   `tracked_ptr`, so they live where one may: on a stack, in a task, in a managed object; in a global or a `std`
   container, a [rooted](../../core/rooted/README.md) of them. [headers](headers/README.md) and [cookie](cookie/README.md) are values.
3. The parser is pure: bytes in, a head or a status to refuse it with out. It holds to RFC 9112 and RFC 9110 and
   refuses everything that has carried request smuggling: a Transfer-Encoding with a Content-Length, a coding other
   than `chunked`, two different lengths, whitespace before a colon, obs-fold, a bare CR, LF alone inside chunked
   framing, a length or a chunk size that is not digits or overflows; the whole list is in the
   [server's rules](server/README.md#the-head). The tests hold each rule to a vector named by its section, and a mutator
   runs over valid requests under ASan.
4. A connection's bytes go through one buffer in front of it, unmanaged memory the connection owns (8 KB, grown for a
   head that does not fit). A head is copied out into a string of its size, whose slices its fields are, one list of
   their number; a body is copied out (read whole: into a vector of its declared length, or gathered and copied
   once), or dropped where it lies when a handler left it. No slice of the buffer leaves it, and it is read only by
   the connection's reads in the task, never on the pool.
5. The client goes through the proxy the environment names, or one given ([proxy](proxy/README.md)): HTTP, HTTP over
   TLS, SOCKS5. It asks for `gzip, deflate, br, zstd` by itself and decodes the body as it is read
   ([uncompressed](response/uncompressed.md)), as Go asks for gzip; a request of its own `Accept-Encoding` gets the
   bytes as they came. The server compresses through the [compression](compression/README.md) middleware.

## Functions

| Function | Header | Description |
|---|---|---|
| [download, async_download](download.md) | `download.h` | a URL saved to a file through a client of the process's; a status but 2xx is an error |
| [is_public_suffix](is_public_suffix.md) | `public_suffix.h` | whether a host is a public suffix of the embedded list: `co.uk`, `github.io` |
| [public_suffix](public_suffix.md) | `public_suffix.h` | the public suffix of a host, `co.uk` for `www.example.co.uk`: Go's `publicsuffix.PublicSuffix` |
| [reason](reason.md) | `status.h` | the reason phrase of a status, `Not Found` for 404 |
| [registrable_domain](registrable_domain.md) | `public_suffix.h` | the site of a host, `example.co.uk` for `www.example.co.uk`: Go's `EffectiveTLDPlusOne` |
| [serve, async_serve](serve.md) | `serve.h` | the files of a directory over HTTP, in one call: ETags, conditional requests, ranges |
| [serve_content](serve_content.md) | `serve.h` | an open file or bytes answered with validators, conditions and ranges: Go's `http.ServeContent` |
| [serve_file](serve_file.md) | `serve.h` | the file at a path answered the same way: Go's `http.ServeFile` |
| [serve_tls](serve_tls.md) | `serve.h` | one handler over https, the certificate and the key read from their files |
| [test_request](test_request.md) | `test.h` | a request as a server hands one to its handler, for a test: Go's `httptest.NewRequest` |

## Classes

| Class | Header | Description |
|---|---|---|
| [basic_auth](basic_auth/README.md) | `auth.h` | Basic authentication of a server (RFC 7617), a middleware with a verify function |
| [body_limit](body_limit/README.md) | `middleware.h` | a body's limit below the server's for a route or a group: 413, Go's `MaxBytesHandler` |
| [cache](cache/README.md) | `cache.h` | a private HTTP cache of a client's responses (RFC 9111): freshness, validation, Vary, stale-while-revalidate and stale-if-error, in memory or in a directory |
| [cache::options](cache-options.md) | `cache.h` | the bytes a cache keeps, a body's limit, the heuristic freshness |
| [client](client/README.md) | `client.h` | HTTP/1.1 and HTTP/2 with a pool of connections, redirects and timeouts: Go's `http.Client` |
| [compression](compression/README.md) | `compression.h` | response compression: zstd, br, gzip or deflate by Accept-Encoding, a flushed body as it goes |
| [compression::options](compression-options.md) | `compression.h` | the codings, the gzip level, the least size and the types of a compression |
| [cookie](cookie/README.md) | `cookie.h` | a cookie, the Set-Cookie field written and read: Go's `http.Cookie` |
| [cookie_jar](cookie_jar/README.md) | `cookie_jar.h` | the cookies of a client's responses, kept by RFC 6265 and the public suffix list, sent with its requests, saved to a file: Go's `cookiejar` |
| [cookie_jar::options](cookie_jar-options.md) | `cookie_jar.h` | the limits of a cookie_jar: cookies in all and per site |
| [cors](cors/README.md) | `middleware.h` | cross-origin requests by the Fetch standard's CORS protocol: preflights, origins, credentials |
| [cors::options](cors-options.md) | `middleware.h` | the origins, the methods, the headers, credentials, the preflight's lifetime of a cors |
| [credentials](credentials.md) | `client.h` | a user and a password a client answers a 401 with: Digest or Basic |
| [csrf](csrf/README.md) | `csrf.h` | cross-site request forgery refused: Go's `CrossOriginProtection`, synchronizer and double-submit tokens |
| [csrf::options](csrf-options.md) | `csrf.h` | the tokens, the trusted origins, the names and the refusal of a csrf |
| [digest_auth](digest_auth/README.md) | `auth.h` | Digest authentication of a server (RFC 7616): SHA-256, SHA-512/256, MD5, auth-int, stateless nonces |
| [digest_auth::options](digest_auth-options.md) | `auth.h` | the algorithms, auth-int and the nonces' lifetime of a digest_auth |
| [download_options](download_options.md) | `download.h` | how a download goes on after its transfer stopped: retries, a part resumed by a later call |
| [event](event.md) | `events.h` | an event of a stream of Server-Sent Events: type, data, id, retry |
| [event_reader](event_reader/README.md) | `events.h` | the events of a stream of Server-Sent Events, read as they come |
| [event_source](event_source/README.md) | `events.h` | a client of Server-Sent Events that reconnects, as a browser's EventSource: retry, Last-Event-ID |
| [event_source::options](event_source-options.md) | `events.h` | the fields, the reconnection time, the reconnections, the limit and the stop of an event_source |
| [event_stream](event_stream/README.md) | `events.h` | a server's stream of Server-Sent Events over a response, each event flushed as it is sent |
| [file_server](file_server/README.md) | `serve.h` | the files of a directory as a handler of a server: Go's `http.FileServer` |
| [form](form/README.md) | `form.h` | a multipart/form-data body to send: fields and files, each file read from disk as it goes out |
| [form::part](form-part.md) | `form.h` | one part of a form: a field, or a file |
| [handler](handler/README.md) | `server.h` | a handler of either kind, what a middleware wraps and passes the request on to |
| [handler::call](handler-call.md) | `server.h` | what calling a handler gives: an awaitable that runs it |
| [headers](headers/README.md) | `headers.h` | the fields of a head, names compared without case and kept as written |
| [multipart_reader](multipart_reader/README.md) | `multipart.h` | a multipart body read part by part as it comes: Go's `multipart.Reader` |
| [multipart_reader::limits](multipart_reader-limits.md) | `multipart.h` | the parts a multipart body may have, the bytes of a part's head |
| [multipart_reader::part](multipart_reader-part.md) | `multipart.h` | what a part's head says: name, filename, content type, fields |
| [proxy](proxy/README.md) | `proxy.h` | which proxy a client's request goes through: the environment's (`http_proxy`, `NO_PROXY`...) or one given |
| [rate_limit](rate_limit/README.md) | `middleware.h` | requests per key, a token bucket each: 429 with Retry-After past them |
| [rate_limit::options](rate_limit-options.md) | `middleware.h` | the key, the idle time and the most keys of a rate_limit |
| [recovery](recovery/README.md) | `middleware.h` | a handler's exception logged through slog and answered 500, or as the program says |
| [recovery::options](recovery-options.md) | `middleware.h` | the logger and the answer of a recovery |
| [request](request/README.md) | `request.h` | a request: made for a client, received by a handler |
| [request_log](request_log/README.md) | `middleware.h` | a record of each exchange through slog, for a route or a group |
| [response](response/README.md) | `response.h` | what a client receives: the status, the fields, the body |
| [response::cache_status](response-cache_status.md) | `response.h` | how the client's cache answered a request: none, miss, hit, revalidated, stale |
| [response_recorder](response_recorder/README.md) | `test.h` | a handler run without the network, what it wrote read back: Go's `httptest.ResponseRecorder` |
| [response_writer](response_writer/README.md) | `response_writer.h` | the response a handler writes: Go's `http.ResponseWriter` |
| [reverse_proxy](reverse_proxy/README.md) | `reverse_proxy.h` | a handler that passes requests on to backends, balanced and health-checked: Go's `httputil.ReverseProxy` |
| [reverse_proxy::backend](reverse_proxy-backend.md) | `reverse_proxy.h` | a backend of a proxy and its counts |
| [reverse_proxy::options](reverse_proxy-options.md) | `reverse_proxy.h` | the hooks, the client, flushing, the forwarding fields, balancing, health, retries of a proxy |
| [serve_options](serve_options.md) | `serve.h` | how files and content are answered: the ETag made, whether ranges are |
| [server](server/README.md) | `server.h` | routes, limits, timeouts, HTTP/2 and shutdown: Go's `http.Server` and `ServeMux` in one |
| [session](session/README.md) | `session.h` | the session of one request: values by name, renew, destroy |
| [sessions](sessions/README.md) | `session.h` | sessions of a server: values sealed in a cookie, or on the server by an id |
| [sessions::options](sessions-options.md) | `session.h` | the cookie and the lifetime of sessions |
| [test_server](test_server/README.md) | `test.h` | a server on the loopback for a test, TLS on a certificate of its own, HTTP/2, a client for it: Go's `httptest.Server` |
| [test_server::options](test_server-options.md) | `test.h` | TLS, HTTP/2 and the requests kept of a test server |
| [websocket](websocket/README.md) | `websocket.h` | a WebSocket connection (RFC 6455, permessage-deflate RFC 7692): a client's connect, a server's accept, messages both ways |
| [websocket::message](websocket-message.md) | `websocket.h` | a WebSocket message received: text or bytes |
| [websocket::options](websocket-options.md) | `websocket.h` | the settings of a WebSocket connection: subprotocols, fields, origins, the limit, keep-alive, compression, a stop |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [csrf::tokens](csrf-tokens.md) | `csrf.h` | the tokens a csrf asks an unsafe request for: none, synchronizer, double-submit |
| [digest_auth::algorithm](digest_auth-algorithm.md) | `auth.h` | the hash algorithms of Digest: MD5, SHA-256, SHA-512/256 and their -sess forms |
| [etag_kind](etag_kind.md) | `serve.h` | how a served representation's ETag is made: none, weak of the metadata, strong of the bytes |
| [reverse_proxy::balancing](reverse_proxy-balancing.md) | `reverse_proxy.h` | how a proxy chooses the backend of a request: in turn, the fewest in progress, at random |
| [suffix_rules](suffix_rules.md) | `public_suffix.h` | the sections of the public suffix list a lookup takes: both, or ICANN's alone |

## Namespaces

| Namespace | Header | Description |
|---|---|---|
| [status](status.md) | `status.h` | the status codes of the IANA registry as `int` constants, `status::ok`, `status::not_found`... |

## Objects and types

| Name | Header | Description |
|---|---|---|
| `dial_function` | `client.h` | `function<async::task<expected<net::connection, io::error>>(const net::url&, async::stop_token)>`: what makes a client's connections, its member `dial` ([client](client/README.md)) |

## See also

- [Benchmarks](benchmarks.md): the server under load, HTTP/1.1, https and HTTP/2, against Go's
- [net](../README.md): the connections and the URLs under it
- [tls](../tls/README.md): https
- [The modules](../../README.md)
