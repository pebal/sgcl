[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [cors](README.md)

# sgcl::net::http::cors::cors

```cpp
cors();                             // (1)
explicit cors(const options& o);    // (2)
```

1. Any origin but `null`, the methods GET, HEAD, PUT, PATCH, POST and DELETE, the headers a preflight asks for, no
   credentials, a preflight kept 5 minutes.
2. The middleware of the options: the lists joined once as they are sent ([cors::options](../cors-options.md)).

## Parameters

| Parameter | Description |
|---|---|
| `o` | the origins, the methods, the headers, credentials, the preflight's lifetime |

## Complexity

Linear in the lists of the options.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::cors());
    srv.route("GET /data", [](net::http::request, net::http::response_writer w) { w.write("data"); });
    auto req = net::http::test_request("GET", "/data");
    req.set_header("Origin", "https://anyone.example");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    println("{} {}", rec.header("Access-Control-Allow-Origin"), rec.body());
}
```

Output:

```text
* data
```

## See also

- [cors](README.md)
