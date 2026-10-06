[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [recovery](recovery/README.md) › options

# sgcl::net::http::recovery::options

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class recovery {
    public:
        struct options {
            optional<slog::logger> log;
            function<void(const request&, response_writer&, const string& what)> answer;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::recovery::options` is where a [recovery](recovery/README.md) logs and how it answers: a plain struct,
its fields set by name.

## Member objects

| Member | Description |
|---|---|
| `log` | the logger of the records, `"handler threw"` at error with `method`, `path` and `error`; slog's default logger when none, the default |
| `answer` | the answer to a request whose handler threw, given the request, its writer (its fields cleared) and what was thrown; a 500 `text/plain` by default. Not called when the head has gone |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    net::http::recovery::options o;
    o.log = slog::logger(kept);
    o.answer = [](const net::http::request&, net::http::response_writer& w, const string&) {
        w.set_status(500);
        w.write("{\"error\":\"internal\"}");
    };
    net::http::server srv;
    srv.use(net::http::recovery(o));
    srv.route("GET /x", [](net::http::request, net::http::response_writer) { throw std::runtime_error("x"); });
    net::http::response_recorder rec;
    rec.serve(srv, net::http::test_request("GET", "/x"));
    println("{} {} {}", rec.status(), rec.body(), kept.size());
}
```

Output:

```text
500 {"error":"internal"} 1
```

## See also

- [recovery](recovery/README.md)
- [sgcl::net::http](README.md)
