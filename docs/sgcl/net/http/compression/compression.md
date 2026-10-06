[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [compression](README.md)

# sgcl::net::http::compression::compression

```cpp
compression();                             // (1)
explicit compression(const options& o);    // (2)
```

1. gzip then deflate, compress's default level, bodies of 1024 bytes and up, the compressible types of the options.
2. The codings, the level, the least size and the types of the options ([compression::options](../compression-options.md));
   a coding the compress module does not have is left out of the list.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the codings, the level, the least size, the types |

## Complexity

Linear in the lists of the options.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::compression({.encodings = {"deflate", "gzip"}, .min_size = 0}));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/plain");
        w.write("short");
    });
    auto req = net::http::test_request("GET", "/");
    req.set_header("Accept-Encoding", "gzip, deflate");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    println("{}", rec.header("Content-Encoding"));
}
```

Output:

```text
deflate
```

## See also

- [compression](README.md)
