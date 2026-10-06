[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::middleware

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class handler;
    class cors;
    class recovery;
    class request_log;
    class body_limit;
    class rate_limit;
    class sessions;      // session.h
    class csrf;          // csrf.h
    class compression;   // compression.h
}
```

A middleware runs around the handlers of a [server](server/README.md): it reads or answers a request before the handler,
adds to the response, or does something once the response has gone. The library's middlewares are classes of the module
— [cors](cors/README.md), [recovery](recovery/README.md), [request_log](request_log/README.md),
[body_limit](body_limit/README.md), [rate_limit](rate_limit/README.md), [sessions](sessions/README.md), [csrf](csrf/README.md),
[basic_auth](basic_auth/README.md), [digest_auth](digest_auth/README.md), [compression](compression/README.md) — and a program's own is a
function. This page is how they fit together; each is on the page of its class.

A middleware goes around every request of a server through [server::use](server/use.md): around the routing, as Go's
`handler = cors(mux)`, so that a request of no route (404, 405, a redirect of the router) goes through it too, and a CORS
preflight reaches [cors](cors/README.md) before the router would answer 405. The first `use` is the outermost. Around
one route it goes through the middleware's `wrap`, Go's `cors(handler)`:
`srv.route("POST /upload", net::http::body_limit(10 << 20).wrap(upload))`; what `wrap` returns is a
[handler](handler/README.md), which another middleware's `wrap` takes in turn.

A program's middleware is one of two functions. A filter, `(request&, response_writer&) -> bool`, runs before the rest:
`true` passes the request on, `false` ends it with what the filter wrote. An around, `(request, response_writer, const
handler& next) -> async::task<>`, is a task that passes the request on with `co_await next(r, w)` and does what it likes
before and after. A plain handler under filters and the library's middlewares runs with no coroutine frame of theirs:
each is a call that returns the task of the rest only when something waits, so the chain costs a few nanoseconds a
middleware; an around is a task, a frame and a resumption each.

## Rules

- The `use` list is read when [serve](server/serve.md) is called, as the server's fields are; a
  [response_recorder](response_recorder/README.md)'s [serve](response_recorder/serve.md) runs it as the server does.
- A middleware sees the request before the router: [path_value](request/path_value.md) is the handler's, not the
  middleware's.
- What a middleware adds to the response's head that depends on the handler (a session's cookie) goes as the head is
  made, at the first flush or at the end, so that a handler that streams has it too.
- The library's middlewares are handles of one word (but `body_limit`, a number): copies share their settings and their
  state (a store of sessions).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::cors({.origins = {"https://app.example"}}));
    srv.use([](net::http::request& req, net::http::response_writer& w) {
        if (req.header("X-Key") != "k1") {
            w.error(net::http::status::unauthorized);
            return false;
        }
        return true;
    });
    srv.use([](net::http::request req, net::http::response_writer w,
               const net::http::handler& next) -> async::task<> {
        w.set_header("X-Before", "yes");
        co_await next(req, w);
        println("{} answered {}", req.url().path(), w.status());
    });
    srv.route("GET /items", [](net::http::request, net::http::response_writer w) { w.write("items\n"); });

    auto req = net::http::test_request("GET", "/items");
    req.set_header("X-Key", "k1");
    req.set_header("Origin", "https://app.example");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    print("{} {} {}", rec.header("Access-Control-Allow-Origin"), rec.header("X-Before"), rec.body());

    net::http::response_recorder refused;
    refused.serve(srv, net::http::test_request("GET", "/items"));
    println("{}", refused.status());
}
```

Output:

```text
/items answered 200
https://app.example yes items
401
```

## See also

- [server::use](server/use.md)
- [handler](handler/README.md)
- [sgcl::net::http](README.md)
