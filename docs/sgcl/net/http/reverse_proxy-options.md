[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [reverse_proxy](reverse_proxy/README.md) › options

# sgcl::net::http::reverse_proxy::options

```cpp
#include "sgcl/net/http/reverse_proxy.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class reverse_proxy {
    public:
        struct options {
            function<void(request& out, const request& in)> rewrite;
            function<void(response_writer& w, const response& backend)> modify_response;
            function<void(response_writer& w, const request& in, const io::error& e)> error_handler;
            http::client client = default_client();
            duration flush_interval = duration::zero();
            size_t buffer_size = 32 * 1024;
            bool preserve_host = false;
            bool x_forwarded = true;
            bool forwarded = false;
            string via;
            balancing policy = balancing::round_robin;
            int max_fails = 3;
            duration fail_timeout = std::chrono::seconds(10);
            string health_path;
            duration health_interval = std::chrono::seconds(10);
            duration health_timeout = std::chrono::seconds(5);
            int retries = 1;
            size_t retry_body_bytes = 64 * 1024;

            static http::client default_client();
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::reverse_proxy::options` is what a [reverse_proxy](reverse_proxy/README.md) does beside passing a
request on: the hooks of Go's `ReverseProxy` (`Rewrite`, `ModifyResponse`, `ErrorHandler`, `Transport` as a
[client](client/README.md), `FlushInterval`), the forwarding fields, and the balancing of nginx's `upstream`. A plain
struct, its fields set by name; the constructors without it take its defaults. The proxy reads it when it is made.

## Member objects

| Member | Description |
|---|---|
| `rewrite` | the outgoing request changed before it goes, the incoming one beside it: its URL ([set_url](request/set_url.md)), its fields, its body. It is made already: the backend's URL with the request's path and query, the fields less the hop-by-hop ones, the forwarding ones, the body as a stream. None by default |
| `modify_response` | the backend's response before it goes back: its status and fields are in the writer, the hop-by-hop ones gone, to be changed; a body written here (or an [error](response_writer/error.md)) replaces the backend's. None by default |
| `error_handler` | what the client gets when the backend cannot be reached or fails before its head; by default 502 Bad Gateway, 504 Gateway Timeout for a timeout, as `text/plain` |
| `client` | how the backends are reached: TLS (roots, a client certificate), HTTP/2 (h2 by ALPN, h2c), a proxy, the timeouts (`timeout` and `response_header_timeout`: past one, 504), the pool. `default_client()` by default: no proxy, not the environment's, and 256 idle connections a backend |
| `flush_interval` | how often what is buffered is flushed while a body comes: zero (the default) when the buffer fills, less than zero after every read. A stream of events (`text/event-stream`) and a body of unknown length are flushed after every read whatever it says |
| `buffer_size` | the block a body is read in, and the bytes buffered before a flush; 32 KB by default |
| `preserve_host` | the incoming Host sent on, where by default the backend's goes |
| `x_forwarded` | X-Forwarded-For with the client's address appended, X-Forwarded-Host and X-Forwarded-Proto replaced by the request's; on by default |
| `forwarded` | Forwarded (RFC 7239) with `for=`, `host=` and `proto=` appended; off by default |
| `via` | Via (RFC 9110 §7.6.3) appended both ways with this name: `1.1 name`; none when empty, the default |
| `policy` | how the backend of a request is chosen ([balancing](reverse_proxy-balancing.md)); in turn by default |
| `max_fails` | the failures in a row after which a backend is skipped for `fail_timeout`; 3 by default, zero never |
| `fail_timeout` | how long a failing backend is skipped; 10 s by default |
| `health_path` | the path of each backend a GET asks every `health_interval`; a 2xx or 3xx within `health_timeout` keeps it in the turn. None when empty, the default; a missing leading slash is added |
| `health_interval` | how often the backends are checked; 10 s by default |
| `health_timeout` | how long a check may take; 5 s by default |
| `retries` | how many other backends an idempotent request (GET, HEAD, OPTIONS, TRACE, PUT, DELETE) whose send failed is tried on; 1 by default |
| `retry_body_bytes` | a body sent again only when it was read whole first: a Content-Length of at most this; 64 KB by default. A body of no length, or a larger one, is streamed and never retried |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server backend([](net::http::request r, net::http::response_writer w) {
        w.write(r.url().path() + " | " + r.header("Via") + " | " + r.header("Forwarded") + "\n");
    });
    net::http::reverse_proxy::options o;
    o.via = "edge";
    o.forwarded = true;
    o.rewrite = [](net::http::request& out, const net::http::request&) {
        out.set_url(out.url().scheme() + "://" + out.url().host() + "/v2" + out.url().path());
    };
    o.modify_response = [](net::http::response_writer& w, const net::http::response& res) {
        w.set_header("X-Backend-Status", to_string(res.status()));
    };
    net::http::reverse_proxy proxy(backend.url(), o);
    net::http::response_recorder rec;
    proxy(net::http::test_request("GET", "/items"), rec.writer()).wait();
    print("{} {}", rec.header("X-Backend-Status"), rec.body());
}
```

Output:

```text
200 /v2/items | 1.1 edge | for=192.0.2.1;host=example.com;proto=http
```

## See also

- [reverse_proxy](reverse_proxy/README.md)
- [balancing](reverse_proxy-balancing.md)
- [client](client/README.md): the settings of `client`
