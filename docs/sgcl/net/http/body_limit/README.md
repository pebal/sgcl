[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::body_limit

```cpp
#include "sgcl/net/http/middleware.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class body_limit;
}
```

A body's limit below the server's `max_body_bytes`, a [middleware](../middleware.md) for a route or a group of them, Go's
`http.MaxBytesHandler`: a request whose Content-Length passes it is answered 413 before the handler runs, and the
connection closed, its body unread; a chunked body that grows past it while read fails the read with
`net::errc::body_too_large`, and a handler that wrote nothing gets a 413 from the server, as with the server's own
limit. A limit of 0 takes no body: a chunked one is refused before it is read, since it cannot be known empty. A limit
above the server's changes nothing: the lower one holds.

## Rules

- A value: the limit alone.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](body_limit.md) | the middleware of a limit |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("POST /note", net::http::body_limit(16).wrap([](net::http::request req,
                                                               net::http::response_writer w) -> async::task<> {
        auto note = co_await req.async_text();
        w.write("kept: " + *note + "\n");
    }));
    for (const char* body : {"buy milk", "a note far longer than sixteen bytes"}) {
        net::http::response_recorder rec;
        rec.serve(srv, net::http::test_request("POST", "/note", body));
        print("{} {}", rec.status(), rec.body());
    }
}
```

Output:

```text
200 kept: buy milk
413 Content Too Large
```

## See also

- [server](../server/README.md): `max_body_bytes`, the limit of every request
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
