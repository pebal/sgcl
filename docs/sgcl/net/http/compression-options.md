[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [compression](compression/README.md) › options

# sgcl::net::http::compression::options

```cpp
#include "sgcl/net/http/compression.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class compression {
    public:
        struct options {
            vector<string> encodings = {"zstd", "br", "gzip", "deflate"};
            compress::level gzip_level;
            compress::brotli::level brotli_level = 4;
            compress::zstd::level zstd_level = 3;
            size_t min_size = 1024;
            vector<string> types = {"text/", "application/json", "application/javascript", "application/xml",
                                    "application/wasm", "image/svg+xml", "application/x-ndjson",
                                    "application/manifest+json"};
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::compression::options` is what a [compression](compression/README.md) compresses and how hard: a plain
struct, its fields set by name. Its defaults are for live responses: compressing costs every response its time, so the
level is the compressor's default rather than its smallest.

## Member objects

| Member | Description |
|---|---|
| `encodings` | the codings offered, in the server's order: the first the client takes with a q above 0 is used; a name the middleware does not know is left out. `{"zstd", "br", "gzip", "deflate"}` by default: a browser that offers zstd gets it, the fastest of the four for the same size |
| `gzip_level` | the level of gzip and deflate ([compress::level](../../compress/level/README.md)): compress's default (6) unless set; `level::fastest` for the least time, `level::smallest` for the least bytes |
| `brotli_level` | br's quality ([compress::brotli::level](../../compress/brotli-level/README.md), 0 to 11): 4 by default, a quality for responses made as they are asked: about libbrotli's 4 in time, between its 4 and 5 in size, whole bodies and flushed ones alike; 5 adds the literals' contexts to pieces of 8 KB and more; 11 is for files compressed once |
| `zstd_level` | zstd's level ([compress::zstd::level](../../compress/zstd-level/README.md)): 3 by default, zstd's own. A level past 19 is given a window of 8 MB, which RFC 9659 lets a client refuse past |
| `min_size` | a whole body below it goes as it is: a few hundred bytes gain less than a coding's head costs. 1024 by default; a flushed body is compressed whatever its size |
| `types` | the compressible Content-Types: a prefix ending in `/` (`"text/"`) or a type, compared without case and parameters |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::compression::options o;
    o.gzip_level = compress::level::fastest;
    o.types = {"application/vnd.api+json"};
    net::http::server srv;
    srv.use(net::http::compression(o));
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "application/vnd.api+json");
        w.write(string("{\"data\":[]}").repeat(200));
    });
    auto req = net::http::test_request("GET", "/");
    req.set_header("Accept-Encoding", "gzip");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    println("{}", rec.header("Content-Encoding"));
}
```

Output:

```text
gzip
```

## See also

- [compression](compression/README.md)
- [sgcl::net::http](README.md)
