[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [compression](README.md)

# sgcl::net::http::compression::wrap

```cpp
handler wrap(const handler& next) const;
```

The middleware around one handler, for a route of its own: the responses of that route compressed, the others as they
are.

## Parameters

| Parameter | Description |
|---|---|
| `next` | the handler the request goes on to |

## Return value

The handler of the middleware and `next`.

## Complexity

Constant.

## Exceptions

What the copy of a function throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto page = [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/html");
        w.write(string("<p>row</p>").repeat(1000));
    };
    net::http::compression gz;
    net::http::server srv;
    srv.route("GET /big", gz.wrap(page));
    srv.route("GET /raw", page);
    for (const char* path : {"/big", "/raw"}) {
        auto req = net::http::test_request("GET", path);
        req.set_header("Accept-Encoding", "gzip");
        net::http::response_recorder rec;
        rec.serve(srv, req);
        println("{} [{}]", path, rec.header("Content-Encoding"));
    }
}
```

Output:

```text
/big [gzip]
/raw []
```

## See also

- [server::use](../server/use.md)
- [compression](README.md)
