[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md)

# sgcl::net::http::cache

```cpp
#include "sgcl/net/http/cache.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class cache;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

A private HTTP cache (RFC 9111) of a [client](../client/README.md), its member `cache`: the responses to the client's
GET requests kept by their URL, and served again by the rules their fields give. A fresh response — within its
`max-age`, its `Expires`, or the heuristic of its `Last-Modified` — is served with no request sent; a stale one is asked
again with its validators (`If-None-Match`, `If-Modified-Since`) and a 304 serves it renewed; `stale-while-revalidate`
serves it at once and asks behind it, `stale-if-error` serves it when the server fails or cannot be reached. In memory,
or in a directory ([on_disk](on_disk.md)) that outlives the program. Go's standard library has none (httpcache is the
usual package); a browser's cache is the model, as curl's has none either.

## Rules

- A private cache: what a response says to a shared cache alone (`s-maxage`, `proxy-revalidate`) is not its, and a
  response with `Cache-Control: private` is stored.
- Stored: the response to a GET whose status is heuristically cacheable (200, 203, 204, 300, 301, 308, 404, 405, 410,
  414, 501) or which has an explicit freshness, and whose body was read to its end. Never stored: `no-store` on either
  side, `Vary: *`, a 206, a request with `Authorization` unless the response says `public` or `must-revalidate`, a body
  past `max_entry_bytes`, a response reached through a redirect.
- Each URL keeps one response per set of the request fields its `Vary` names; a HEAD is answered from what a GET
  stored. A body the client decoded (`decompress`) is kept decoded.
- The request's own `Cache-Control` is honoured: `no-cache` and `max-age` ask for a validation, `max-stale` and
  `min-fresh` move the line, `only-if-cached` answers 504 when nothing fits, `no-store` keeps the cache out.
- A POST, PUT, PATCH or DELETE that succeeds drops what the cache holds of its URL and of the `Location` and
  `Content-Location` of the same origin.
- How each response came is its [from_cache](../response/from_cache.md); a served one carries its `Age`.
- A handle of one word: copies, and the clients they are given to, share the entries; safe from many tasks and
  threads at once.

## Member types

| Type | Definition |
|---|---|
| [options](../cache-options.md) | the limits of the bytes kept and the heuristic freshness |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](cache.md) | a cache in memory |
| [on_disk](on_disk.md) | a cache in a directory (static) |
| [size](size.md) | how many responses the cache holds |
| [bytes](bytes.md) | the bytes of their bodies |
| [clear](clear.md) | drops every entry |
| [erase](erase.md) | drops the entries of a URL |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

#include <atomic>

using namespace sgcl;

int main() {
    std::atomic<int> asked{0};
    net::http::server srv;
    srv.route("GET /news", [&asked](net::http::request, net::http::response_writer w) {
        ++asked;
        w.set_header("Cache-Control", "max-age=60");
        w.write("news " + to_string(asked.load()));
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string url = "http://127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/news";

    net::http::client web;
    web.cache = net::http::cache();
    for (int i : range(3)) {
        auto res = web.get(url);
        println("{} {}", res->text().value(), res->from_cache() == net::http::response::cache_status::hit);
    }
    println("the server asked {} time(s)", asked.load());
    srv.close();
}
```

Output:

```text
news 1 false
news 1 true
news 1 true
the server asked 1 time(s)
```

## See also

- [client](../client/README.md): `cache`
- [response::from_cache](../response/from_cache.md)
