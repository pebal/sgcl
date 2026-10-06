[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::compression

```cpp
#include "sgcl/net/http/compression.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class compression;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Response compression, a [middleware](../middleware.md): the response of a client that accepts a coding (Accept-Encoding,
its q-values read by RFC 9110 §12.5.3) gets its body in it — zstd (RFC 8878, its window 8 MB at most as RFC 9659 asks), br (RFC 7932), gzip or deflate
(zlib's framing, as RFC 9110 names it),
the server's order deciding among the codings the client takes — when its Content-Type is one of the compressible types
and its body is at least `min_size`. A response that flushes is compressed as it goes, each flush a sync flush the client
decodes at once (a stream of events stays a stream). Go's standard library has none (gziphandler is the usual package);
nginx's gzip module is the same idea.

## Rules

- `Vary: Accept-Encoding` goes on every response of a compressible type, compressed or not: a cache keys its answer by
  the field.
- A compressed response loses its Content-Length (the server writes the new one, or chunks a flushed body),
  `Accept-Ranges` (ranges of the coding are not offered) and its strong ETag's strength (`W/` prepended: the bytes are
  not the identity's).
- Not compressed: a HEAD, a status without a body (1xx, 204, 304), a 206 (a range of the identity), a response the
  handler gave a Content-Encoding of its own, a type not in the list, a whole body under `min_size`, a client that
  takes no coding of the list (it gets identity: the server never answers 406 for codings).
- A file the handler wrote ([write](../response_writer/write.md)) goes through memory when compressed, not by
  `sendfile`.
- A handle of one word: copies share the settings.

## Member types

| Type | Definition |
|---|---|
| [options](../compression-options.md) | the codings, their levels, the least size, the types |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](compression.md) | the middleware of the options |
| [wrap](wrap.md) | the middleware around one handler |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.use(net::http::compression());
    srv.route("GET /report", [](net::http::request, net::http::response_writer w) {
        w.set_header("Content-Type", "text/csv");
        w.write(string("date,amount\n2026-10-01,100\n").repeat(500));
    });
    auto req = net::http::test_request("GET", "/report");
    req.set_header("Accept-Encoding", "gzip, deflate");
    net::http::response_recorder rec;
    rec.serve(srv, req);
    println("{} {}", rec.header("Content-Encoding"), rec.header("Vary"));
    auto csv = compress::gzip::decompress(rec.body());
    println("{} bytes from {}", csv->size(), rec.body().size());
}
```

Sample output:

```text
gzip Accept-Encoding
14000 bytes from 105
```

## See also

- [compression::options](../compression-options.md)
- [client](../client/README.md): `decompress`, the other side
- [middleware](../middleware.md)
- [sgcl::net::http](../README.md)
